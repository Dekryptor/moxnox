#pragma once

#include "unique_handle.h"

class process : public object {
public:
    process() : object(object_type::process) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool signalled = false;
    unique_handle img;
};

static_assert(is_sync_object<process>);
