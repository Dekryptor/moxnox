#include <stdint.h>

using NTSTATUS = int32_t;
using USHORT = uint16_t;
using PWSTR = char16_t*;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

extern "C" NTSTATUS NtDisplayString(PUNICODE_STRING String);

extern "C"
void startup() {
    static const char16_t msg[] = u"hello, world";

    UNICODE_STRING us;
    us.Length = us.MaximumLength = sizeof(msg) - sizeof(char16_t);
    us.Buffer = (PWSTR)msg;

    NtDisplayString(&us);

    while (1) { }
}
