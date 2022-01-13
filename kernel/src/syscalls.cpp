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
