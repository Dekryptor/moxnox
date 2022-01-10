#include <stdint.h>

using NTSTATUS = int32_t;
using USHORT = uint16_t;
using PWSTR = char16_t*;
using HANDLE = uintptr_t;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

extern "C" NTSTATUS NtDisplayString(PUNICODE_STRING String);
extern "C" NTSTATUS NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status);

static constexpr HANDLE NtCurrentThread() {
    return (HANDLE)-2;
}

extern "C"
void startup() {
    static const char16_t msg[] = u"hello, world\n";

    UNICODE_STRING us;
    us.Length = us.MaximumLength = sizeof(msg) - sizeof(char16_t);
    us.Buffer = (PWSTR)msg;

    NtDisplayString(&us);

    NtTerminateThread(NtCurrentThread(), 42);
}
