import print;
import ucrtbase;

static int errno = 0; // FIXME

extern "C" __declspec(dllexport)
_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler handler) {
    // FIXME?

    return nullptr;
}

extern "C"
int* __errno() {
    // FIXME - actually should be thread-local

    return &errno;
}
