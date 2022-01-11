#pragma once

#include <stdint.h>

enum nt_status : uint32_t {
    SUCCESS = 0x00000000,
    PENDING = 0x00000103,
    NOT_IMPLEMENTED = 0xc0000002,
    ACCESS_VIOLATION = 0xc0000005,
    INVALID_HANDLE = 0xc0000008,
    OBJECT_TYPE_MISMATCH = 0xc0000024,
    INSUFFICIENT_RESOURCES = 0xc000009a
};

using NTSTATUS = enum nt_status;
using USHORT = uint16_t;
using PWSTR = char16_t*;
using HANDLE = uintptr_t;

static consteval HANDLE NtCurrentProcess() {
    return (HANDLE)-1;
}

static consteval HANDLE NtCurrentThread() {
    return (HANDLE)-2;
}

struct UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char16_t* Buffer;
};

using PUNICODE_STRING = UNICODE_STRING*;

#ifndef NO_NT_SYSCALLS

extern "C" {

NTSTATUS NtDisplayString(const UNICODE_STRING* string);
NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status);
NTSTATUS NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status);

};

#endif
