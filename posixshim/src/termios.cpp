import nt;
import ucrtbase;
import print;

#undef errno
import errno;

struct termios; // FIXME

using speed_t = unsigned int;

extern "C"
int tcgetattr(int /*fd*/, termios* /*p*/) {
    print("FIXME - tcgetattr\n");

    *_errno() = EINVAL;
    return -1;
}

extern "C"
int tcsetattr(int /*fd*/, int /*optional_actions*/, const termios* /*p*/) {
    print("FIXME - tcsetattr\n");

    *_errno() = EINVAL;
    return -1;
}

extern "C"
speed_t cfgetospeed(const termios* /*p*/) {
    print("FIXME - cfgetospeed\n");

    return 0; // FIXME
}
