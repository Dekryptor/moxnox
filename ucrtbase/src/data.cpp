import print;
import ucrtbase;

static int fmode = _O_TEXT;

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
    print("FIXME - __p__commode\n");
    return nullptr;
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
