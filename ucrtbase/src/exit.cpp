#include <utility>
#include <array>

import ucrtbase;
import print;
import ntdll;

using namespace std;

static array<_PVFV, 64> atexit_funcs;
static unsigned int num_atexit_funcs;

extern "C" __declspec(dllexport)
int _crt_atexit(_PVFV function) {
    // FIXME - do we need a lock, or is this only ever called from first thread?

    if (num_atexit_funcs == atexit_funcs.size())
        return -1;

    atexit_funcs[num_atexit_funcs] = function;
    num_atexit_funcs++;

    return 0;
}

extern "C" __declspec(dllexport)
void exit(int code) noexcept {
    RtlExitUserProcess(code);
    unreachable();
}
