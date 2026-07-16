#pragma once

#include <sys/types.h>

struct iovec {
    size_t iov_len;
    void* iov_base;
};

ssize_t readv(int fd, const struct iovec* iov, int iovcnt);
ssize_t writev(int fd, const struct iovec* iov, int iovcnt);
