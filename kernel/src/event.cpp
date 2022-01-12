#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"
#include "spinlock.h"

using namespace std;

unique_handle create_event(bool autoreset) {
    auto eptr = new event;
    if (!eptr) {
        print("new failed\n");
        return {};
    }

    auto& e = *eptr;
    e.autoreset = autoreset;

    auto hv = add_kernel_handle(e, 0); // FIXME - access mask

    e.release(); // add_kernel_handle increases refcount

    return unique_handle{hv};
}

void set_event(event& e) {
    unsigned int release_count = 0;

    // signal waiting threads

    {
        sync_lock sl;
        lock_guard lock(sl);

        {
            auto le = thread_list.next;

            while (le != &thread_list) {
                auto& thr = containing_record<thread>(le);

                if (thr.status == thread_status::waiting && thr.waiting_for.has_value() && &thr.waiting_for->get() == static_cast<object*>(&e)) {
                    thr.status = thread_status::runnable;
                    thr.waiting_for = nullopt;
                    release_count++;

                    if (e.autoreset)
                        break;
                }

                le = le->next;
            }
        }

        // if autoreset, only signal if nothing waiting when fired
        if (!e.autoreset || release_count == 0)
            e.signalled = true;
    }

    for (unsigned int i = 0; i < release_count; i++) {
        e.release();
    }
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

    set_event(e);

    e.release();
}

void reset_event(handle_t h) {
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

    {
        sync_lock sl;
        lock_guard lock(sl);

        e.signalled = false;
    }

    e.release();
}

void event::on_wake() {
    if (autoreset)
        signalled = false;
}
