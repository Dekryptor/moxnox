module;

#include <span>
#include <functional>
#include <atomic>
#include <optional>
import objects;
#include "thread.h"
#include "spinlock.h"
#include "handle.h"
#include "scheduler.h"

export module event;

import print;
import list;
import bsod;
import object_ref;

using namespace std;

export class event : public sync_object {
public:
    event() : sync_object(object_type::event, false) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void on_wake();
    void set_holding_sync_lock();

    bool autoreset;
};

static_assert(is_sync_object<event>);
static_assert(object_has_on_wake<event>);

export object_ref create_event(bool autoreset) {
    auto eptr = new event;
    if (!eptr) {
        print("new failed\n");
        return {};
    }

    auto& e = *eptr;
    e.autoreset = autoreset;

    return e;
}

void event::set_holding_sync_lock() {
    // signal waiting threads

    auto waiter_count = wake_up_waiters(waiters);

    // if autoreset, only signal if nothing waiting when fired
    if (!autoreset || waiter_count == 0)
        signalled = true;
}

export void set_event(event& e) {
    sync_lock sl;
    lock_guard lock(sl);

    e.set_holding_sync_lock();
}

export void reset_event(event& e) {
    sync_lock sl;
    lock_guard lock(sl);

    e.signalled = false;
}

void event::on_wake() {
    if (autoreset)
        signalled = false;
}
