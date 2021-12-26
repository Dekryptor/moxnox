#pragma once

#include "volume.h"

class volume : public object {
public:
    volume() : object(object_type::volume) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    size_t read(std::span<uint8_t> s, size_t offset);
};

static_assert(is_readable_object_with_offset<volume>);
