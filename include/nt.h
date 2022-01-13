#pragma once

#include <stdint.h>

enum nt_status : uint32_t {
    SUCCESS = 0x00000000,
    PENDING = 0x00000103,
    NOT_IMPLEMENTED = 0xc0000002,
    ACCESS_VIOLATION = 0xc0000005,
    INVALID_HANDLE = 0xc0000008,
    INVALID_PARAMETER = 0xc000000d,
    OBJECT_TYPE_MISMATCH = 0xc0000024,
    INSUFFICIENT_RESOURCES = 0xc000009a
};

using NTSTATUS = enum nt_status;
using USHORT = uint16_t;
using PWSTR = char16_t*;
using HANDLE = uintptr_t;
using ULONG = uint32_t;
using LONG = int32_t;
using PVOID = void*;
using ULONG_PTR = uintptr_t;
using LONGLONG = int64_t;

static consteval HANDLE NtCurrentProcess() {
    return (HANDLE)-1;
}

static consteval HANDLE NtCurrentThread() {
    return (HANDLE)-2;
}

static constexpr bool success(NTSTATUS val) {
    return !((uint32_t)val & 0x80000000);
}

struct UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char16_t* Buffer;
};

struct IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
};

using PIO_APC_ROUTINE = void (*)(PVOID, const IO_STATUS_BLOCK*, ULONG);

union LARGE_INTEGER {
    struct {
        ULONG LowPart;
        LONG HighPart;
    };
    LONGLONG QuadPart;
};

extern "C" {

NTSTATUS NtDisplayString(const UNICODE_STRING* string);
NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status);
NTSTATUS NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status);
NTSTATUS NtWriteFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                     void* apc_context, IO_STATUS_BLOCK* io_status_block, const void* buffer,
                     uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t* key);
NTSTATUS NtResumeProcess(HANDLE proc_handle);
NTSTATUS NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                 size_t* region_size, uint32_t allocation_type, uint32_t protect);

};
