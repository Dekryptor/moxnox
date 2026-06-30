#include <unistd.h>
#include <errno.h>

extern "C"
long sysconf(int name) {
    switch (name) {
        case _SC_CLK_TCK:
            return 1000;

        default:
            *_errno() = EINVAL;
            return -1;
    }
}
