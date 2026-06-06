#include <unistd.h>

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
    unsigned char *p = (unsigned char *)buf;
    size_t done = 0;

    (void)flags;

    while (done < buflen) {
        size_t n = buflen - done;

        if (n > 256)
            n = 256;

        if (getentropy(p + done, n) != 0)
            return -1;

        done += n;
    }

    return (ssize_t)done;
}
