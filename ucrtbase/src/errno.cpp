import print;
import ucrtbase;

static thread_local int errno = 0;

extern "C"
_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler /*handler*/) {
    // FIXME?

    return nullptr;
}

extern "C"
int* __errno() {
    return &errno;
}
