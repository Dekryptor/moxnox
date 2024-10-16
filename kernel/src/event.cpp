#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "handle.h"

using namespace std;

object_ref create_event(bool autoreset) {
    auto eptr = new event;
    if (!eptr) {
        printk("new failed\n");
        return {};
    }

    auto& e = *eptr;
    e.autoreset = autoreset;

    return e;
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

void reset_event(event& e) {
    sync_lock sl;
    lock_guard lock(sl);

    e.signalled = false;
}

void event::on_wake() {
    if (autoreset)
        signalled = false;
}
