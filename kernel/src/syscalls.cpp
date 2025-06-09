module;

#include "kernel.h"
#include "nt.h"
#include "handle.h"
#include "spinlock.h"
import rbtree;
import event;
import pipe;
import timer;
import objects;
#include "thread.h"
import page_cache;
#include "inode.h"
import volume;
#include "visit_object.h"

export module syscalls;

import format;
import print;
import string;
import config;

using namespace std;

// syscalls.S
export {
    extern "C" void init_syscalls();
}

extern "C"
NTSTATUS NtDisplayString(const UNICODE_STRING* string) {
    auto u16sv = u16string_view(string->Buffer, string->Length / sizeof(char16_t));
    auto buflen = utf16_to_utf8_len(u16sv);

    if (buflen == 0)
        return nt_status::SUCCESS;

    auto buf = (char*)malloc(buflen);
    if (!buf)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(buf, buflen), u16sv);

    print("{}", string_view(buf, buflen));

    free(buf);

    return nt_status::SUCCESS;
}

static bool copy_from_process(void* ks_addr, const void* us_addr, size_t size) {
    auto t = get_current_thread();

    return t->proc->copy_from(ks_addr, us_addr, size);
}

static bool copy_to_process(void* us_addr, const void* ks_addr, size_t size) {
    auto t = get_current_thread();

    return t->proc->copy_to(us_addr, ks_addr, size);
}

static bool copy_unicode_string_from_process(const UNICODE_STRING* us_us, UNICODE_STRING& ks_us) {
    if (!copy_from_process(&ks_us, us_us, sizeof(UNICODE_STRING)))
        return false;

    if (ks_us.Buffer) {
        auto buf = (char16_t*)malloc(ks_us.Length);
        if (!buf)
            return false;

        if (!copy_from_process(buf, ks_us.Buffer, ks_us.Length)) {
            free(buf);
            return false;
        }

        ks_us.Buffer = buf;
    }

    return true;
}

extern "C"
NTSTATUS user_NtDisplayString(const UNICODE_STRING* string) {
    UNICODE_STRING us;

    // FIXME - needs TCB privilege

    if (!copy_unicode_string_from_process(string, us))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtDisplayString(&us);

    if (us.Buffer)
        free(us.Buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status) {
    return NtTerminateThread(thread_handle, exit_status);
}

extern "C"
NTSTATUS user_NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status) {
    return NtTerminateProcess(process_handle, exit_status);
}

template<size_t N>
static bool copy_stack_args(thread& t, array<uintptr_t, N>& args) {
    // 1 uint64_t for return address, followed by 4 for shadow store
    return copy_from_process(args.data(), (void*)((uintptr_t)t.syscall_context.Rsp + (5 * sizeof(uint64_t))),
                             args.size() * sizeof(uintptr_t));
}

extern "C"
NTSTATUS user_NtWriteFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                          void* apc_context) {
    auto& t = *get_current_thread();
    IO_STATUS_BLOCK iosb;
    array<uintptr_t, 5> args;
    LARGE_INTEGER byte_offset;
    uint32_t key;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto us_iosb = (IO_STATUS_BLOCK*)args[0];
    auto us_buf = (void*)args[1];
    auto length = (uint32_t)args[2];
    auto us_byte_offset = (LARGE_INTEGER*)args[3];
    auto us_key = (uint32_t*)args[4];

    if (!us_iosb)
        return nt_status::INVALID_PARAMETER;

    void* buf = nullptr;

    // FIXME - can we map buffer directly rather than copying?

    if (us_buf) {
        buf = malloc(length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(buf, us_buf, length)) {
            free(buf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    if (us_byte_offset) {
        if (!copy_from_process(&byte_offset, us_byte_offset, sizeof(LARGE_INTEGER))) {
            if (buf)
                free(buf);

            return nt_status::ACCESS_VIOLATION;
        }
    }

    if (us_key) {
        if (!copy_from_process(&key, us_key, sizeof(uint32_t))) {
            if (buf)
                free(buf);

            return nt_status::ACCESS_VIOLATION;
        }
    }

    auto ret = NtWriteFile(file_handle, event, apc_routine, apc_context, &iosb, buf, length,
                           us_byte_offset ? &byte_offset : nullptr, us_key ? &key : nullptr);

    if (buf)
        free(buf);

    if (success(ret)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtResumeProcess(HANDLE proc_handle) {
    return NtResumeProcess(proc_handle);
}

extern "C"
NTSTATUS user_NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                      size_t* region_size) {
    auto& t = *get_current_thread();
    array<uintptr_t, 2> args;

    if (!region_size || !base_address)
        return nt_status::INVALID_PARAMETER;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto allocation_type = (uint32_t)args[0];
    auto protect = (uint32_t)args[1];
    void* addr;
    size_t size;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtAllocateVirtualMemory(proc_handle, &addr, zero_bits, &size, allocation_type, protect);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtFreeVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                  uint32_t free_type) {
    if (!region_size || !base_address)
        return nt_status::INVALID_PARAMETER;

    void* addr;
    size_t size;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtFreeVirtualMemory(proc_handle, &addr, &size, free_type);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtReadFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                         void* apc_context) {
    auto& t = *get_current_thread();
    IO_STATUS_BLOCK iosb;
    array<uintptr_t, 5> args;
    LARGE_INTEGER byte_offset;
    uint32_t key;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto us_iosb = (IO_STATUS_BLOCK*)args[0];
    auto us_buf = (void*)args[1];
    auto length = (uint32_t)args[2];
    auto us_byte_offset = (LARGE_INTEGER*)args[3];
    auto us_key = (uint32_t*)args[4];

    if (!us_iosb)
        return nt_status::INVALID_PARAMETER;

    void* buf = nullptr;

    // FIXME - can we map buffer directly rather than copying?

    if (length > 0) {
        buf = malloc(length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (us_byte_offset) {
        if (!copy_from_process(&byte_offset, us_byte_offset, sizeof(LARGE_INTEGER))) {
            if (buf)
                free(buf);

            return nt_status::ACCESS_VIOLATION;
        }
    }

    if (us_key) {
        if (!copy_from_process(&key, us_key, sizeof(uint32_t))) {
            if (buf)
                free(buf);

            return nt_status::ACCESS_VIOLATION;
        }
    }

    auto ret = NtReadFile(file_handle, event, apc_routine, apc_context, &iosb, buf, length,
                          us_byte_offset ? &byte_offset : nullptr, us_key ? &key : nullptr);

    if (success(ret)) {
        if (iosb.Information > 0) {
            if (!copy_to_process(us_buf, buf, iosb.Information))
                ret = nt_status::ACCESS_VIOLATION;
        }

        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    if (buf)
        free(buf);

    return ret;
}

extern "C"
NTSTATUS user_NtProtectVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                     uint32_t new_prot) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;

    if (!region_size || !base_address)
        return nt_status::INVALID_PARAMETER;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto us_old_prot = (uint32_t*)args[0];

    if (!us_old_prot)
        return nt_status::ACCESS_VIOLATION;

    void* addr;
    size_t size;
    uint32_t old_prot;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtProtectVirtualMemory(proc_handle, &addr, &size, new_prot, &old_prot);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_old_prot, &old_prot, sizeof(uint32_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
void unimplemented_syscall(const char* name) {
    auto& t = *get_current_thread();
    bool written = false;
    char s[255];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Unimplemented syscall {}.\n", name);

    if (t.proc) {
        auto objrw_stderr = t.proc->get_stderr();

        if (objrw_stderr) {
            uint64_t written_bytes;

            // FIXME - make sure process has permissions to write to handle

            visit_obj(objrw_stderr.get(), [&](auto&& o) {
                if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>) {
                    o.write(span((uint8_t*)s, (size_t)(sp.data() - s)), written_bytes); // FIXME - handle short writes
                    written = true;
                }
            });
        }
    }

    if (!written)
        print("{}", string_view(s, (size_t)(sp.data() - s)));

    NtTerminateProcess(NtCurrentProcess(), nt_status::NOT_IMPLEMENTED);
}

static NTSTATUS copy_object_attributes(OBJECT_ATTRIBUTES& oa, UNICODE_STRING& us, const OBJECT_ATTRIBUTES* us_oa) {
    if (!copy_from_process(&oa, us_oa, sizeof(OBJECT_ATTRIBUTES)))
        return nt_status::ACCESS_VIOLATION;

    if (oa.Length < sizeof(OBJECT_ATTRIBUTES))
        return nt_status::INVALID_PARAMETER;

    if (oa.ObjectName) {
        if (!copy_from_process(&us, oa.ObjectName, sizeof(UNICODE_STRING)))
            return nt_status::ACCESS_VIOLATION;

        // FIXME - make sure no denial of service if us.Length is ludicrous value (max value?)

        if (us.Length == 0)
            us.Buffer = nullptr;
        else {
            auto buf = (char16_t*)malloc(us.Length);
            if (!buf)
                return nt_status::INSUFFICIENT_RESOURCES;

            if (!copy_from_process(buf, us.Buffer, us.Length)) {
                free(buf);
                return nt_status::ACCESS_VIOLATION;
            }

            us.Buffer = buf;
        }

        oa.ObjectName = &us;
    }

    // FIXME - SecurityDescriptor
    // FIXME - SecurityQualityOfService

    return nt_status::SUCCESS;
}

static void free_object_attributes(OBJECT_ATTRIBUTES& oa) {
    if (oa.ObjectName && oa.ObjectName->Buffer)
        free(oa.ObjectName->Buffer);

    // FIXME - SecurityDescriptor
    // FIXME - SecurityQualityOfService
}

extern "C"
NTSTATUS user_NtCreateFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                           const OBJECT_ATTRIBUTES* object_attributes, IO_STATUS_BLOCK* io_status_block) {
    auto& t = *get_current_thread();
    array<uintptr_t, 7> args;
    HANDLE h;
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER alloc;
    void* eabuf = nullptr;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS ret;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    if (!file_handle || !io_status_block)
        return nt_status::INVALID_PARAMETER;

    auto allocation_size = (const LARGE_INTEGER*)args[0];
    auto file_attributes = (uint32_t)args[1];
    auto share_access = (uint32_t)args[2];
    auto create_disposition = (uint32_t)args[3];
    auto create_options = (uint32_t)args[4];
    auto ea_buffer = (void*)args[5];
    auto ea_length = (uint32_t)args[6];

    if (allocation_size) {
        if (!copy_from_process(&alloc, allocation_size, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (ea_length > 0) {
        eabuf = malloc(ea_length);
        if (!eabuf)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(eabuf, ea_buffer, ea_length)) {
            free(eabuf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    ret = copy_object_attributes(oa, us, object_attributes);
    if (!success(ret)) {
        if (eabuf)
            free(eabuf);

        return ret;
    }

    ret = NtCreateFile(&h, desired_access, &oa, &iosb, allocation_size ? &alloc : nullptr,
                       file_attributes, share_access, create_disposition, create_options,
                       eabuf, ea_length);

    if (eabuf)
        free(eabuf);

    free_object_attributes(oa);

    if (success(ret)) {
        if (!copy_to_process(file_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(io_status_block, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtClose(HANDLE handle) {
    return NtClose(handle);
}

extern "C"
NTSTATUS user_NtCreateSection(HANDLE* section_handle, ACCESS_MASK desired_access,
                              const OBJECT_ATTRIBUTES* object_attributes, const LARGE_INTEGER* maximum_size) {
    auto& t = *get_current_thread();
    array<uintptr_t, 3> args;
    LARGE_INTEGER size;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    if (!section_handle)
        return nt_status::INVALID_PARAMETER;

    auto section_page_protection = (uint32_t)args[0];
    auto allocation_attributes = (uint32_t)args[1];
    auto file_handle = (HANDLE)args[2];

    if (maximum_size) {
        if (!copy_from_process(&size, maximum_size, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (object_attributes) {
        auto ret = copy_object_attributes(oa, us, object_attributes);
        if (!success(ret))
            return ret;
    }

    auto ret = NtCreateSection(&h, desired_access, object_attributes ? &oa : nullptr,
                               maximum_size ? &size : nullptr, section_page_protection,
                               allocation_attributes, file_handle);

    if (object_attributes)
        free_object_attributes(oa);

    if (success(ret)) {
        if (!copy_to_process(section_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtMapViewOfSection(HANDLE section_handle, HANDLE process_handle, void** base_address,
                                 uintptr_t zero_bits) {
    auto& t = *get_current_thread();
    array<uintptr_t, 6> args;
    void* addr;
    LARGE_INTEGER offset;
    size_t size;

    if (!base_address)
        return nt_status::INVALID_PARAMETER;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto commit_size = (size_t)args[0];
    auto section_offset = (LARGE_INTEGER*)args[1];
    auto view_size = (size_t*)args[2];
    auto inherit_disposition = (enum SECTION_INHERIT)args[3];
    auto allocation_type = (uint32_t)args[4];
    auto win32_protect = (uint32_t)args[5];

    if (!view_size)
        return nt_status::INVALID_PARAMETER;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (section_offset) {
        if (!copy_from_process(&offset, section_offset, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (!copy_from_process(&size, view_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtMapViewOfSection(section_handle, process_handle, &addr, zero_bits,
                                  commit_size, section_offset ? &offset : nullptr,
                                  &size, inherit_disposition,
                                  allocation_type, win32_protect);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (section_offset) {
            if (!copy_to_process(section_offset, &offset, sizeof(LARGE_INTEGER)))
                ret = nt_status::ACCESS_VIOLATION;
        }

        if (!copy_to_process(view_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtUnmapViewOfSection(HANDLE process_handle, void* base_address) {
    return NtUnmapViewOfSection(process_handle, base_address);
}

extern "C"
NTSTATUS user_NtQueryDirectoryFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                                   void* apc_context) {
    auto& t = *get_current_thread();
    array<uintptr_t, 7> args;
    IO_STATUS_BLOCK iosb;
    void* info;
    UNICODE_STRING file_name;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto us_iosb = (IO_STATUS_BLOCK*)args[0];
    auto us_file_information = (void*)args[1];
    auto length = (uint32_t)args[2];
    auto fic = (FILE_INFORMATION_CLASS)args[3];
    auto return_single_entry = (bool)args[4];
    auto us_file_name = (const UNICODE_STRING*)args[5];
    auto restart_scan = (bool)args[6];

    if (!file_handle)
        return nt_status::INVALID_PARAMETER;

    if (!us_iosb || !us_file_information)
        return nt_status::INVALID_PARAMETER;

    if (us_file_name) {
        if (!copy_unicode_string_from_process(us_file_name, file_name))
            return nt_status::ACCESS_VIOLATION;
    }

    if (length > 0) {
        info = malloc(length);
        if (!info) {
            if (us_file_name && file_name.Buffer)
                free(file_name.Buffer);

            return nt_status::INSUFFICIENT_RESOURCES;
        }
    } else
        info = nullptr;

    auto ret = NtQueryDirectoryFile(file_handle, event, apc_routine, apc_context, &iosb, info, length,
                                    fic, return_single_entry, us_file_name ? &file_name : nullptr,
                                    restart_scan);

    if (success(ret)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;

        if (iosb.Information > 0) {
            if (!copy_to_process(us_file_information, info, iosb.Information))
                ret = nt_status::ACCESS_VIOLATION;
        }
    }

    if (info)
        free(info);

    if (us_file_name && file_name.Buffer)
        free(file_name.Buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtCreateThread(HANDLE* us_thread_handle, ACCESS_MASK desired_access,
                             const OBJECT_ATTRIBUTES* object_attributes, HANDLE process_handle) {
    auto& t = *get_current_thread();
    array<uintptr_t, 4> args;
    NTSTATUS status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT thread_context;
    INITIAL_TEB initial_teb;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto us_client_id = (CLIENT_ID*)args[0];
    auto us_thread_context = (const CONTEXT*)args[1];
    auto us_initial_teb = (const INITIAL_TEB*)args[2];
    auto create_suspended = (bool)args[3];

    if (!us_thread_handle || !us_client_id || !us_thread_context || !us_initial_teb)
        return nt_status::INVALID_PARAMETER;


    if (!copy_from_process(&thread_context, us_thread_context, sizeof(CONTEXT)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&initial_teb, us_initial_teb, sizeof(INITIAL_TEB)))
        return nt_status::ACCESS_VIOLATION;

    if (object_attributes) {
        status = copy_object_attributes(oa, us, object_attributes);
        if (!success(status))
            return status;
    }

    status = NtCreateThread(&h, desired_access, object_attributes ? &oa : nullptr, process_handle,
                            &client_id, &thread_context, &initial_teb, create_suspended);

    if (success(status)) {
        if (!copy_to_process(us_thread_handle, &h, sizeof(HANDLE)))
            status = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_client_id, &client_id, sizeof(CLIENT_ID)))
            status = nt_status::ACCESS_VIOLATION;
    }

    if (object_attributes)
        free_object_attributes(oa);

    return status;
}

extern "C"
NTSTATUS user_NtResumeThread(HANDLE thread_handle, uint32_t* suspend_count) {
    uint32_t count;

    auto status = NtResumeThread(thread_handle, suspend_count ? &count : 0);

    if (success(status)) {
        if (suspend_count) {
            if (!copy_to_process(suspend_count, &count, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    return status;
}

extern "C"
NTSTATUS user_NtWaitForSingleObject(HANDLE object_handle, bool alertable, const LARGE_INTEGER* us_time_out) {
    LARGE_INTEGER time_out;

    // FIXME - are NtCurrentProcess() or NtThreadProcess() valid values for object_handle?

    if (us_time_out) {
        if (!copy_from_process(&time_out, us_time_out, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    return NtWaitForSingleObject(object_handle, alertable, us_time_out ? &time_out : nullptr);
}

extern "C"
NTSTATUS user_NtCreateProcess(HANDLE* process_handle, ACCESS_MASK desired_access,
                              const OBJECT_ATTRIBUTES* object_attributes, HANDLE parent_process) {
    auto& t = *get_current_thread();
    array<uintptr_t, 4> args;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto inherit_object_table = (bool)args[0];
    auto section_handle = (HANDLE)args[1];
    auto debug_port = (HANDLE)args[2];
    auto exception_port = (HANDLE)args[3];

    if (!process_handle)
        return nt_status::INVALID_PARAMETER;

    if (object_attributes) {
        status = copy_object_attributes(oa, us, object_attributes);
        if (!success(status))
            return status;
    }

    status = NtCreateProcess(&h, desired_access, object_attributes ? &oa : nullptr, parent_process,
                             inherit_object_table, section_handle, debug_port, exception_port);

    if (success(status)) {
        if (!copy_to_process(process_handle, &h, sizeof(HANDLE)))
            status = nt_status::ACCESS_VIOLATION;
    }

    if (object_attributes)
        free_object_attributes(oa);

    return status;
}

extern "C"
NTSTATUS user_NtDuplicateObject(HANDLE source_process_handle, HANDLE source_handle, HANDLE target_process_handle,
                                HANDLE* target_handle) {
    auto& t = *get_current_thread();
    array<uintptr_t, 3> args;
    NTSTATUS status;
    HANDLE h;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto desired_access = (ACCESS_MASK)args[0];
    auto handle_attributes = (uint32_t)args[1];
    auto options = (uint32_t)args[2];

    status = NtDuplicateObject(source_process_handle, source_handle, target_process_handle, target_handle ? &h : nullptr,
                               desired_access, handle_attributes, options);

    if (success(status)) {
        if (target_handle) {
            if (!copy_to_process(target_handle, &h, sizeof(HANDLE)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    return status;
}

extern "C"
NTSTATUS user_NtReadVirtualMemory(HANDLE process_handle, void* base_address, void* buffer, uint32_t buffer_size) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;
    NTSTATUS status;
    void* buf;
    uint32_t read;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto number_of_bytes_read = (uint32_t*)args[0];

    if (!buffer)
        return nt_status::INVALID_PARAMETER;

    if (buffer_size > 0) {
        buf = malloc(buffer_size);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtReadVirtualMemory(process_handle, base_address, buffer_size > 0 ? buf : nullptr, buffer_size,
                                 number_of_bytes_read ? &read : nullptr);

    if (success(status)) {
        if (buffer_size > 0) {
            if (!copy_to_process(buffer, buf, buffer_size))
                status = nt_status::ACCESS_VIOLATION;
        }

        if (number_of_bytes_read) {
            if (!copy_to_process(number_of_bytes_read, &read, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (buffer_size > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtWriteVirtualMemory(HANDLE process_handle, void* base_address, void* buffer, uint32_t buffer_size) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;
    NTSTATUS status;
    void* buf;
    uint32_t written;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto number_of_bytes_written = (uint32_t*)args[0];

    if (!buffer)
        return nt_status::INVALID_PARAMETER;

    if (buffer_size > 0) {
        buf = malloc(buffer_size);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(buf, buffer, buffer_size)) {
            free(buf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    status = NtWriteVirtualMemory(process_handle, base_address, buffer_size > 0 ? buf : nullptr, buffer_size,
                                  number_of_bytes_written ? &written : nullptr);

    if (success(status)) {
        if (number_of_bytes_written) {
            if (!copy_to_process(number_of_bytes_written, &written, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (buffer_size > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtQueryInformationProcess(HANDLE process_handle, enum PROCESSINFOCLASS process_information_class,
                                        void* process_information, uint32_t process_information_length) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;
    NTSTATUS status;
    void* buf;
    uint32_t retlen;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto return_length = (uint32_t*)args[0];

    if (!process_information)
        return nt_status::INVALID_PARAMETER;

    if (process_information_length > 0) {
        buf = malloc(process_information_length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtQueryInformationProcess(process_handle, process_information_class,
                                       process_information_length > 0 ? buf : nullptr,
                                       process_information_length, &retlen);

    if (success(status)) {
        if (retlen > 0) {
            if (!copy_to_process(process_information, buf, retlen))
                status = nt_status::ACCESS_VIOLATION;
        }

        if (return_length) {
            if (!copy_to_process(return_length, &retlen, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (process_information_length > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtQuerySection(HANDLE section_handle, enum SECTION_INFORMATION_CLASS section_information_class,
                             void* section_information, size_t section_information_length) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;
    NTSTATUS status;
    void* buf;
    size_t retlen;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto return_length = (size_t*)args[0];

    if (!section_information)
        return nt_status::INVALID_PARAMETER;

    if (section_information_length > 0) {
        buf = malloc(section_information_length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtQuerySection(section_handle, section_information_class,
                            section_information_length > 0 ? buf : nullptr,
                            section_information_length, &retlen);

    if (success(status)) {
        if (retlen > 0) {
            if (!copy_to_process(section_information, buf, retlen))
                status = nt_status::ACCESS_VIOLATION;
        }

        if (return_length) {
            if (!copy_to_process(return_length, &retlen, sizeof(size_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (section_information_length > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtQueryInformationThread(HANDLE thread_handle, enum THREAD_INFORMATION_CLASS thread_information_class,
                                       void* thread_information, uint32_t thread_information_length) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;
    NTSTATUS status;
    void* buf;
    uint32_t retlen;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto return_length = (uint32_t*)args[0];

    if (!thread_information)
        return nt_status::INVALID_PARAMETER;

    if (thread_information_length > 0) {
        buf = malloc(thread_information_length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtQueryInformationThread(thread_handle, thread_information_class,
                                      thread_information_length > 0 ? buf : nullptr,
                                      thread_information_length, &retlen);

    if (success(status)) {
        if (retlen > 0) {
            if (!copy_to_process(thread_information, buf, retlen))
                status = nt_status::ACCESS_VIOLATION;
        }

        if (return_length) {
            if (!copy_to_process(return_length, &retlen, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (thread_information_length > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtQuerySystemTime(LARGE_INTEGER* SystemTime) {
    LARGE_INTEGER time;
    NTSTATUS status;

    status = NtQuerySystemTime(&time);
    if (!success(status))
        return status;

    if (!copy_to_process(SystemTime, &time, sizeof(time)))
        status = nt_status::ACCESS_VIOLATION;

    return status;
}

extern "C"
NTSTATUS user_NtCreateTimer(HANDLE* timer_handle, ACCESS_MASK desired_access,
                            const OBJECT_ATTRIBUTES* object_attributes,
                            TIMER_TYPE timer_type) {
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;

    if (object_attributes) {
        auto ret = copy_object_attributes(oa, us, object_attributes);
        if (!success(ret))
            return ret;
    }

    auto ret = NtCreateTimer(&h, desired_access, object_attributes ? &oa : nullptr,
                             timer_type);

    if (object_attributes)
        free_object_attributes(oa);

    if (success(ret)) {
        if (!copy_to_process(timer_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtSetTimer(HANDLE timer_handle, const LARGE_INTEGER* due_time,
                         PTIMER_APC_ROUTINE timer_apc_routine, void* timer_context) {
    auto& t = *get_current_thread();
    array<uintptr_t, 3> args;
    LARGE_INTEGER ks_due_time;
    bool ks_previous_state;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto resume_timer = (bool)args[0];
    auto period = (int32_t)args[1];
    auto previous_state = (bool*)args[2];

    if (!copy_from_process(&ks_due_time, due_time, sizeof(ks_due_time)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtSetTimer(timer_handle, &ks_due_time, timer_apc_routine,
                          timer_context, resume_timer, period,
                          previous_state ? &ks_previous_state : nullptr);

    if (success(ret)) {
        if (previous_state && !copy_to_process(previous_state, &ks_previous_state, sizeof(ks_previous_state)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtCancelTimer(HANDLE timer_handle, bool* current_state) {
    bool ks_current_state;

    auto ret = NtCancelTimer(timer_handle,
                             current_state ? &ks_current_state : nullptr);

    if (success(ret)) {
        if (current_state && !copy_to_process(current_state, &ks_current_state, sizeof(ks_current_state)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    // FIXME - check user has SeShutdownPrivilege

    return NtShutdownSystem(action);
}

extern "C"
NTSTATUS NtDeviceIoControlFile(HANDLE file_handle, HANDLE /*event*/, PIO_APC_ROUTINE /*apc_routine*/, void* /*apc_context*/,
                               IO_STATUS_BLOCK* iosb, uint32_t control_code, const void* input_buffer,
                               uint32_t input_buffer_len, void* output_buffer, uint32_t output_buffer_len) {
    if (!file_handle)
        return nt_status::INVALID_PARAMETER;

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    NTSTATUS ret;

    visit_obj(objrw.get(), [&](auto&& o) {
        if constexpr (has_ioctls<decltype(o)>) {
            ret = o.ioctl(control_code, span((uint8_t*)input_buffer, input_buffer_len),
                          span((uint8_t*)output_buffer, output_buffer_len),
                          iosb->Information);
        } else if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>) {
            visit_obj(o.ino, [&](auto&& o) {
                if constexpr (has_ioctls<decltype(o)>) {
                    ret = o.ioctl(control_code, span((uint8_t*)input_buffer, input_buffer_len),
                                  span((uint8_t*)output_buffer, output_buffer_len),
                                  iosb->Information);
                }
            });
        } else
            ret = nt_status::NOT_SUPPORTED;
    });

    return ret;
}

extern "C"
NTSTATUS user_NtDeviceIoControlFile(HANDLE file_handle, HANDLE event,
                                    PIO_APC_ROUTINE apc_routine, void* apc_context) {
    auto& t = *get_current_thread();
    array<uintptr_t, 6> args;
    IO_STATUS_BLOCK iosb;
    void* input_buffer = nullptr;
    void* output_buffer = nullptr;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto user_iosb = (IO_STATUS_BLOCK*)args[0];
    auto control_code = (uint32_t)args[1];
    auto user_input_buffer = (const void*)args[2];
    auto input_buffer_len = (uint32_t)args[3];
    auto user_output_buffer = (void*)args[4];
    auto output_buffer_len = (uint32_t)args[5];

    if (!user_iosb)
        return nt_status::INVALID_PARAMETER;

    if (user_input_buffer && input_buffer_len > 0) {
        input_buffer = malloc(input_buffer_len);
        if (!input_buffer)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(input_buffer, user_input_buffer, input_buffer_len))
            return nt_status::ACCESS_VIOLATION;
    }

    if (user_output_buffer && output_buffer_len > 0) {
        output_buffer = malloc(output_buffer_len);
        if (!output_buffer) {
            if (input_buffer)
                free(input_buffer);

            return nt_status::INSUFFICIENT_RESOURCES;
        }
    }

    auto ret = NtDeviceIoControlFile(file_handle, event, apc_routine, apc_context,
                                     &iosb, control_code, input_buffer, input_buffer_len,
                                     output_buffer, output_buffer_len);

    if (input_buffer)
        free(input_buffer);

    if (output_buffer) {
        if (!copy_to_process(user_output_buffer, output_buffer, output_buffer_len)) {
            free(output_buffer);
            return nt_status::ACCESS_VIOLATION;
        }

        free(output_buffer);
    }

    if (!copy_to_process(user_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
        return nt_status::ACCESS_VIOLATION;

    return ret;
}

extern "C"
void dispatch_signal() {
    auto& t = *get_current_thread();
    auto& p = *t.proc;

    __asm __volatile("sti");

    uintptr_t us_context = t.syscall_context.Rsp - sizeof(CONTEXT);

    if (!p.copy_to((void*)us_context, &t.syscall_context, sizeof(CONTEXT))) {
        print("Unable to copy thread context to stack.\n");
        t.exit_status = nt_status::ACCESS_VIOLATION;
        t.terminate();
        return;
    }

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)t.pending_signal;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)t.syscall_context.Rip;
    er.NumberParameters = 0;

    size_t er_size = offsetof(EXCEPTION_RECORD, ExceptionInformation) + (sizeof(uintptr_t) * er.NumberParameters);
    uintptr_t us_er = us_context - er_size;

    if (!p.copy_to((void*)us_er, &er, er_size)) {
        print("Unable to copy exception record to stack.\n");
        t.exit_status = nt_status::ACCESS_VIOLATION;
        t.terminate();
        return;
    }

    t.syscall_context.Rip = p.ntdll_addr + mn_signal;
    t.syscall_context.Rcx = us_er;
    t.syscall_context.Rdx = us_context;
    t.syscall_context.Rsp -= sizeof(CONTEXT);
    t.syscall_context.Rsp -= er_size;

    __asm __volatile("cli");
}

extern "C"
NTSTATUS user_NtSetInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* us_iosb, void* us_buffer,
                                   uint32_t buffer_size) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;
    IO_STATUS_BLOCK iosb;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto file_information_class = (FILE_INFORMATION_CLASS)args[0];

    if (!us_iosb || !us_buffer || buffer_size == 0)
        return nt_status::INVALID_PARAMETER;

    buffer_size = min(buffer_size, (uint32_t)PAGE_SIZE);

    auto buffer = malloc(buffer_size);
    if (!buffer)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (!copy_from_process(buffer, us_buffer, buffer_size)) {
        free(buffer);
        return nt_status::ACCESS_VIOLATION;
    }

    auto ret = NtSetInformationFile(file_handle, &iosb, buffer, buffer_size,
                                    file_information_class);

    if (success(ret)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtQueryInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* us_iosb, void* us_buffer,
                                     uint32_t buffer_size) {
    auto& t = *get_current_thread();
    array<uintptr_t, 1> args;
    IO_STATUS_BLOCK iosb;

    if (!copy_stack_args(t, args))
        return nt_status::ACCESS_VIOLATION;

    auto file_information_class = (FILE_INFORMATION_CLASS)args[0];

    if (!us_iosb || !us_buffer || buffer_size == 0)
        return nt_status::INVALID_PARAMETER;

    buffer_size = min(buffer_size, (uint32_t)PAGE_SIZE);

    auto buffer = malloc(buffer_size);
    if (!buffer)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto ret = NtQueryInformationFile(file_handle, &iosb, buffer, buffer_size,
                                      file_information_class);

    if (success(ret)) {
        if (!copy_to_process(us_buffer, buffer, iosb.Information))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    free(buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtFlushVirtualMemory(HANDLE proc_handle, void** base_address,
                                   size_t* region_size, IO_STATUS_BLOCK* iosb) {
    if (!region_size || !base_address || !iosb)
        return nt_status::INVALID_PARAMETER;

    void* addr;
    size_t size;
    IO_STATUS_BLOCK ks_iosb;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtFlushVirtualMemory(proc_handle, &addr, &size, &ks_iosb);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(iosb, &ks_iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}
