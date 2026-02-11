import print;
import ucrtbase;

static int fmode = _O_TEXT;
static unsigned int commode = 0;

extern "C" __declspec(dllexport)
void _set_app_type(_crt_app_type _Type) {
    print("FIXME - _set_app_type\n");
}

extern "C" __declspec(dllexport)
int* __p___argc() {
    print("FIXME - __p___argc\n");
    return nullptr;
}

extern "C" __declspec(dllexport)
char*** __p___argv() {
    print("FIXME - __p___argv\n");
    return nullptr;
}

extern "C" __declspec(dllexport)
unsigned int* __p__commode() {
    return &commode;
}

extern "C" __declspec(dllexport)
char*** __p__environ() {
    print("FIXME - __p__environ\n");
    return nullptr;
}

extern "C" __declspec(dllexport)
int* __p__fmode() {
    return &fmode;
}

extern "C" __declspec(dllexport)
void _initterm(_PVFV* first, _PVFV* last) {
    print("FIXME - _initterm\n");
}

extern "C" __declspec(dllexport)
int _initterm_e(_PIFV* first, _PIFV* last) {
    print("FIXME - _initterm_e\n");

    return 0;
}
