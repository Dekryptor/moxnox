#include "ntdll_private.h"

using namespace std;

extern "C" __declspec(dllexport)
void mn_signal(EXCEPTION_RECORD* exception, CONTEXT* context) {
    char s[1024];
    auto sp = span(s, sizeof(s));

    // FIXME - call SEH handler if present

    // FIXME - print EXE name

    switch ((NTSTATUS)exception->ExceptionCode) {
        case nt_status::ACCESS_VIOLATION:
            // FIXME - translate error code to descriptive message
            sp = sp_strcpy(sp, "Page fault (address = ");
            sp = sp_itoa_hex(sp, exception->ExceptionInformation[1]);
            sp = sp_strcpy(sp, ", error code = ");
            sp = sp_itoa_hex(sp, exception->ExceptionInformation[0]);
            sp = sp_strcpy(sp, ")\n");
        break;

        case nt_status::INTEGER_DIVIDE_BY_ZERO:
            sp = sp_strcpy(sp, "Divide by zero\n");
        break;

        default:
            sp = sp_strcpy(sp, "Unhandled exception ");
            sp = sp_itoa_hex(sp, exception->ExceptionCode, 8);
            sp = sp_strcpy(sp, "\n");
        break;
    }

    sp = sp_context(sp, *context);
    print(string_view(s, sp.data() - s));

    // FIXME - print stacktrace? With DWARF information?

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
}
