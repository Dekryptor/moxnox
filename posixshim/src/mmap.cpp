#include <sys/mman.h>
#include <errno.h>

import print;

extern "C"
void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t off) {
    print("FIXME - mmap\n");

    *_errno() = EINVAL;
    return MAP_FAILED;
}
