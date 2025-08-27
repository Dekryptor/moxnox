module;

#include <span>
#include <functional>
#include <atomic>
#include <optional>
import objects;
#include "spinlock.h"
#include "scheduler.h"
#include "handle.h"

export module event;

import print;
import list;
import bsod;
import object_ref;
import thread;

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

extern "C"
NTSTATUS NtCreateEvent(HANDLE* event_handle, ACCESS_MASK desired_access,
                       const OBJECT_ATTRIBUTES* /*object_attributes*/,
                       EVENT_TYPE event_type, bool initial_state) {
    bool autoreset;
    auto& t = *get_current_thread();
    handle_t h;

    // FIXME - object_attributes

    switch (event_type) {
        case EVENT_TYPE::SynchronizationEvent:
            autoreset = true;
            break;

        case EVENT_TYPE::NotificationEvent:
            autoreset = false;
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    auto obj = create_event(autoreset);
    if (!obj)
        return nt_status::INTERNAL_ERROR;

    auto& e = static_cast<event&>(obj.get());
    e.signalled = initial_state;

    auto status = t.proc->add_handle(obj.get(), desired_access, true, h);
    if (!success(status))
        return status;

    *event_handle = h;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtSetEvent(HANDLE event_handle, int32_t* /*previous_state*/) {
    // FIXME - previous_state

    auto objrw = deref_handle(event_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - check access mask

    auto& obj = objrw.get();

    if (obj.type != object_type::event)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& e = static_cast<event&>(obj);

    set_event(e);

    return nt_status::SUCCESS;
}
