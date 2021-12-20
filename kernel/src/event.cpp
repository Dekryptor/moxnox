#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"

using namespace std;

unique_handle create_event() {
    auto eptr = new event;
    if (!eptr) {
        print("new failed\n");
        return {};
    }

    auto& e = *eptr;

    auto hv = add_handle(e, 0); // FIXME - access mask

    e.release(); // add_handle increases refcount

    return unique_handle{hv};
}

void set_event(handle_t h) {
    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        print("No such handle\n");
        return;
    }

    auto& obj = objrw->get();

    if (obj.type != object_type::event) {
        // FIXME - exception
        print("Not an event\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return;
    }

    auto& e = static_cast<event&>(objrw->get());
    unsigned int release_count = 0;

    // signal waiting threads

    acquire_sync_lock();

    {
        auto le = thread_list.next;

        while (le != &thread_list) {
            auto& thr = containing_record<thread>(le);

            if (thr.status == thread_status::waiting && thr.waiting_for.has_value() && &thr.waiting_for->get() == static_cast<object*>(&e)) {
                thr.status = thread_status::runnable;
                thr.waiting_for = nullopt;
                release_count++;
            }

            le = le->next;
        }
    }

    e.signalled = true;

    release_sync_lock();

    for (unsigned int i = 0; i < release_count; i++) {
        e.release();
    }

    e.release();
}

// FIXME - reset_event (and auto-reset events)
