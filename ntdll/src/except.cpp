#include "ntdll_private.h"

extern "C" __declspec(dllexport)
void mn_signal(EXCEPTION_RECORD* exception, CONTEXT* context) {
    print("FIXME - mn_signal\n");

    // FIXME - call SEH handler if present
    // FIXME - otherwise, print context and die

    // FIXME - print stacktrace? With DWARF information?

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)0xffffffff); // FIXME
}
