#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"
#include "spinlock.h"

using namespace std;

unique_handle create_semaphore(unsigned int maximum_count) {
    auto sptr = new semaphore;
    if (!sptr) {
        print("new failed\n");
        return {};
    }

    auto& s = *sptr;
    s.maximum_count = maximum_count;
    s.current_count = 0;
    s.signalled = true;

    auto hv = add_handle(s, 0); // FIXME - access mask

    s.release(); // add_handle increases refcount

    // FIXME - what happens if a thread dies before calling release_semaphore?

    return unique_handle{hv};
}

void release_semaphore(semaphore& s) {
    unsigned int release_count = 0;

    // FIXME - what if not held by current thread?

    {
        lock_guard l1(sync_lock);
        lock_guard l2(s.lock);

        // signal waiting threads

        s.current_count--;

        {
            auto le = thread_list.next;

            while (le != &thread_list) {
                auto& thr = containing_record<thread>(le);

                if (thr.status == thread_status::waiting && thr.waiting_for.has_value() && &thr.waiting_for->get() == static_cast<object*>(&s)) {
                    thr.status = thread_status::runnable;
                    thr.waiting_for = nullopt;
                    release_count++;
                    break;
                }

                le = le->next;
            }
        }

        if (release_count == 0)
            s.signalled = true;
    }

    if (release_count == 1)
        s.release();
}

void release_semaphore(handle_t h) {
    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        print("No such handle\n");
        return;
    }

    auto& obj = objrw->get();

    if (obj.type != object_type::semaphore) {
        // FIXME - exception
        print("Not a semaphore\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return;
    }

    auto& s = static_cast<semaphore&>(objrw->get());

    release_semaphore(s);

    s.release();
}

// FIXME - reset_event

void semaphore::on_wake() {
    lock_guard l(lock);

    current_count++;

    if (current_count == maximum_count)
        signalled = false;
}
