#pragma once

#include "spinlock.h"

class semaphore : public object {
public:
    semaphore() : object(object_type::semaphore) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool signalled = false;
    unsigned int maximum_count;
    unsigned int current_count;
    spinlock lock;
};

static_assert(is_sync_object<semaphore>);
