#include <stdint.h>
#include <nt.h>

extern "C"
void startup() {
    static const char16_t msg[] = u"hello, world\n";

    UNICODE_STRING us;
    us.Length = us.MaximumLength = sizeof(msg) - sizeof(char16_t);
    us.Buffer = (PWSTR)msg;

    NtDisplayString(&us);

    NtTerminateThread(NtCurrentThread(), (NTSTATUS)42);
}
