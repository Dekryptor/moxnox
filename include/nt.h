#pragma once

#include <stdint.h>

enum nt_status : uint32_t {
    SUCCESS = 0x00000000,
    NOT_IMPLEMENTED = 0xc0000002,
    ACCESS_VIOLATION = 0xc0000005,
    INSUFFICIENT_RESOURCES = 0xc000009a
};

using NTSTATUS = enum nt_status;
using USHORT = uint16_t;
using PWSTR = char16_t*;
using HANDLE = uintptr_t;

static constexpr HANDLE NtCurrentThread() {
    return (HANDLE)-2;
}

struct UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char16_t* Buffer;
};

using PUNICODE_STRING = UNICODE_STRING*;

#ifndef NO_NT_SYSCALLS

extern "C" NTSTATUS NtDisplayString(const UNICODE_STRING* string);
extern "C" NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status);

#endif
