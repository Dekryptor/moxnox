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

    void on_wake();

    bool signalled = false;
    bool autoreset;
};

static_assert(is_sync_object<event>);
static_assert(object_has_on_wake<timer>);
