#include <stdint.h>

import nt;
import ntdll;
import kernelbase;

struct ctrl_handler {
    ctrl_handler* next;
    PHANDLER_ROUTINE func;
};

static ctrl_handler* handlers = nullptr;

extern "C" __declspec(dllexport)
uint32_t CtrlRoutine(void* arg) {
    auto param = (uint32_t)(uintptr_t)arg;

    auto h = handlers;
    while (h) {
        if (h->func(param))
            return 0;

        h = h->next;
    }

    NtTerminateProcess(NtCurrentProcess(), nt_status::CONTROL_C_EXIT);

    return 0;
}

extern "C" __declspec(dllexport)
bool SetConsoleCtrlHandler(PHANDLER_ROUTINE func, bool add) {
    // FIXME - critical section

    if (!func) {
        // FIXME - disabling or enabling Ctrl+C handling
    } else if (add) {
        auto h = (ctrl_handler*)RtlAllocateHeap(GetProcessHeap(), 0, sizeof(ctrl_handler));
        if (!h)
            return false;

        h->next = handlers;
        h->func = func;
        handlers = h;

        return true;
    } else {
        // FIXME - removing from list
    }

    return false;
}
