#pragma once

#include <sys/types.h>

struct iovec {
    size_t iov_len;
    void* iov_base;
};
