#include <utility>
#include <array>
#include <stdint.h>

import ucrtbase;
import ntdll;
import print;

using namespace std;

static array<_PVFV, 64> atexit_funcs;
static size_t num_atexit_funcs = 0;

extern "C"
int _crt_atexit(_PVFV function) {
    // FIXME - do we need a lock, or is this only ever called from first thread?

    if (num_atexit_funcs == atexit_funcs.size())
        return -1;

    atexit_funcs[num_atexit_funcs] = function;
    num_atexit_funcs++;

    return 0;
}

extern "C"
void _cexit() {
    // FIXME - locks?

    for (size_t i = 0; i < num_atexit_funcs; i++) {
        atexit_funcs[num_atexit_funcs - i - 1]();
    }
}

extern "C"
void exit(int code) noexcept {
    _cexit();

    RtlExitUserProcess((uint32_t)code);
    unreachable();
}

using _tls_callback_type = void (*)(void* dll_base, uint32_t reason,
                                    void* reserved);

extern "C"
void _register_thread_local_exe_atexit_callback(_tls_callback_type /*callback*/) {
    print("FIXME - _register_thread_local_exe_atexit_callback\n"); // FIXME
}
