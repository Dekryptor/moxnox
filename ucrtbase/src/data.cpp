import print;
import ucrtbase;

static int fmode = _O_TEXT;
static unsigned int commode = 0;
static int argc = 0; // FIXME - populate from GetCommandLineW
static char** argv = nullptr; // FIXME - populate from GetCommandLineW

extern "C" __declspec(dllexport)
void _set_app_type(_crt_app_type _Type) {
    print("FIXME - _set_app_type\n");
}

extern "C" __declspec(dllexport)
int* __p___argc() {
    return &argc;
}

extern "C" __declspec(dllexport)
char*** __p___argv() {
    return &argv;
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

extern "C" __declspec(dllexport)
int _initialize_narrow_environment() {
    return 0;
}

extern "C" __declspec(dllexport)
int _configure_narrow_argv(int /*mode*/) {
    return 0;
}
