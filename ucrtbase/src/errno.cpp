import print;
import ucrtbase;

extern "C" __declspec(dllexport)
_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler handler) {
    print("FIXME - _set_invalid_parameter_handler\n");

    return nullptr;
}

extern "C" __declspec(dllexport)
int* _errno() {
    print("FIXME - _errno\n");

    return nullptr;
}
