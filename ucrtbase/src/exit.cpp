#include <utility>

import ucrtbase;
import print;
import ntdll;

using namespace std;

extern "C" __declspec(dllexport)
int _crt_atexit(_PVFV function) {
    print("FIXME - _crt_atexit\n");

    return 0;
}

extern "C" __declspec(dllexport)
void exit(int code) {
    RtlExitUserProcess(code);
    unreachable();
}
