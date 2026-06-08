#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#undef errno
import errno;

extern "C"
int getentropy(void* buffer, size_t length) {
    if (length > GETENTROPY_MAX) {
        *_errno() = EIO;
        return -1;
    }

    // FIXME - this should be higher quality

    auto b = (uint8_t*)buffer;

    while (length >= sizeof(int)) {
        *(int*)b = rand();

        b += sizeof(int);
        length -= sizeof(int);
    }

    if (length != 0) {
        auto r = rand();

        while (length > 0) {
            *b = (uint8_t)(r & 0xff);

            b++;
            length--;
            r >>= 8;
        }
    }

    return 0;
}
