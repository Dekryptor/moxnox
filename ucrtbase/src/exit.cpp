import ucrtbase;
import print;

extern "C" __declspec(dllexport)
int _crt_atexit(_PVFV function) {
    print("FIXME - _crt_atexit\n");

    return 0;
}
