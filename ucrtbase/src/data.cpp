import print;
import ucrtbase;

extern "C" __declspec(dllexport)
void _set_app_type(_crt_app_type _Type) {
    print("FIXME - _set_app_type\n");
}
