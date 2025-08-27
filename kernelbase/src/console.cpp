#include <nt.h>
#include <kernelbase.h>

import format;
import print;

extern "C" __declspec(dllexport)
uint32_t CtrlRoutine(void* /*arg*/) {
    // FIXME

    return 0;
}

extern "C" __declspec(dllexport)
bool SetConsoleCtrlHandler(PHANDLER_ROUTINE func, bool add) {
    print("FIXME - SetConsoleCtrlHandler\n");

    return false;
}
