import ucrtbase;
import print;

#undef errno
import errno;

extern "C"
char* _getcwd(char *buffer, int maxlen) {
    print("FIXME - _getcwd\n");

    // FIXME

    *_errno() = EINVAL;

    return nullptr;
}
