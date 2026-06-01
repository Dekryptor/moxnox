#include <stdint.h>

import nt;
import ntdll;
import kernelbase;

struct ctrl_handler {
    ctrl_handler* next;
    HANDLER_ROUTINE func;
};

static ctrl_handler* handlers = nullptr;

extern "C"
bool SetConsoleCtrlHandler(HANDLER_ROUTINE func, bool add) {
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
