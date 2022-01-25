#include "kernel.h"
#include "nt.h"
#include "objects.h"

using namespace std;

NTSTATUS NtDisplayString(const UNICODE_STRING* string) {
    auto u16sv = u16string_view(string->Buffer, string->Length / sizeof(char16_t));
    auto buflen = utf16_to_utf8_len(u16sv);

    if (buflen == 0)
        return nt_status::SUCCESS;

    auto buf = (char*)malloc(buflen);
    if (!buf)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(buf, buflen), u16sv);

    print(string_view(buf, buflen));

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
    if (thread_handle & kernel_handle_mask && thread_handle != NtCurrentThread())
        return nt_status::INVALID_HANDLE;

    return NtTerminateThread(thread_handle, exit_status);
}

extern "C"
NTSTATUS user_NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status) {
    if (process_handle & kernel_handle_mask && process_handle != NtCurrentProcess())
        return nt_status::INVALID_HANDLE;

    return NtTerminateProcess(process_handle, exit_status);
}

template<size_t N>
static bool copy_stack_args(thread& t, array<uintptr_t, N>& args) {
    // 1 uint64_t for return address, followed by 4 for shadow store
    return copy_from_process(args.data(), (void*)((uintptr_t)t.rsp_before_syscall + (5 * sizeof(uint64_t))),
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

    if (file_handle & kernel_handle_mask || event & kernel_handle_mask)
        return nt_status::INVALID_HANDLE;

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
    if (proc_handle & kernel_handle_mask && proc_handle != NtCurrentProcess())
        return nt_status::INVALID_HANDLE;

    return NtResumeProcess(proc_handle);
}

extern "C"
NTSTATUS user_NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                      size_t* region_size) {
    auto& t = *get_current_thread();
    array<uintptr_t, 2> args;

    if (!region_size || !base_address)
        return nt_status::INVALID_PARAMETER;

    if (proc_handle & kernel_handle_mask && proc_handle != NtCurrentProcess())
        return nt_status::INVALID_HANDLE;

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

    if (proc_handle & kernel_handle_mask && proc_handle != NtCurrentProcess())
        return nt_status::INVALID_HANDLE;

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

    if (file_handle & kernel_handle_mask || event & kernel_handle_mask)
        return nt_status::INVALID_HANDLE;

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

    if (proc_handle & kernel_handle_mask && proc_handle != NtCurrentProcess())
        return nt_status::INVALID_HANDLE;

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
    print("Unimplemented syscall ");
    print(name);
    print(".\n");

    NtTerminateProcess(NtCurrentProcess(), nt_status::NOT_IMPLEMENTED);
}

static NTSTATUS copy_object_attributes(OBJECT_ATTRIBUTES& oa, UNICODE_STRING& us, const OBJECT_ATTRIBUTES* us_oa) {
    if (!copy_from_process(&oa, us_oa, sizeof(OBJECT_ATTRIBUTES)))
        return nt_status::ACCESS_VIOLATION;

    if (oa.Length < sizeof(OBJECT_ATTRIBUTES))
        return nt_status::INVALID_PARAMETER;

    if (oa.RootDirectory & kernel_handle_mask)
        return nt_status::INVALID_HANDLE;

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
    if (handle & kernel_handle_mask)
        return nt_status::INVALID_HANDLE;

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

    if (file_handle & kernel_handle_mask)
        return nt_status::INVALID_HANDLE;

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
    array<uintptr_t, 5> args;
    void* addr;
    LARGE_INTEGER offset;
    size_t size;

    if (section_handle & kernel_handle_mask)
        return nt_status::INVALID_HANDLE;

    if (process_handle & kernel_handle_mask && process_handle != NtCurrentProcess())
        return nt_status::INVALID_HANDLE;

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
