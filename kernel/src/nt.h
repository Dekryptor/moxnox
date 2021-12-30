#pragma once

enum nt_status : uint32_t {
    SUCCESS = 0x00000000,
    NOT_IMPLEMENTED = 0xc0000002
};

using NTSTATUS = enum nt_status;

struct UNICODE_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char16_t* Buffer;
};
