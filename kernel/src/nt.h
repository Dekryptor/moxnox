#pragma once

enum nt_status : uint32_t {
    SUCCESS = 0x00000000,
    NOT_IMPLEMENTED = 0xc0000002,
    ACCESS_VIOLATION = 0xc0000005,
    INSUFFICIENT_RESOURCES = 0xc000009a
};

using NTSTATUS = enum nt_status;

struct UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char16_t* Buffer;
};

using HANDLE = uintptr_t;
