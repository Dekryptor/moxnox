#pragma once

class event : public object {
public:
    event() : object(object_type::event) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool signalled = false;
};

static_assert(is_sync_object<event>);
