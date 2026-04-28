import nt;
import ucrtbase;
import print;

#undef errno
import errno;

struct termios; // FIXME

extern "C"
int tcgetattr(int fd, termios* p) {
    print("FIXME - tcgetattr\n");

    *_errno() = EINVAL;
    return -1;
}
