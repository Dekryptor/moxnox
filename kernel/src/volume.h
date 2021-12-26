#pragma once

#include "volume.h"

class volume : public object {
public:
    volume() : object(object_type::volume) { }

    virtual ~volume() {
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;

    virtual size_t read(std::span<uint8_t> s, uint64_t offset) = 0;

    void probe();
};

static_assert(is_readable_object_with_offset<volume>);
