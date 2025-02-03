#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "handle.h"

using namespace std;

object_ref create_semaphore(unsigned int maximum_count) {
    auto sptr = new semaphore;
    if (!sptr) {
        printk("new failed\n");
        return {};
    }

    auto& s = *sptr;
    s.maximum_count = maximum_count;
    s.current_count = 0;
    s.signalled = true;

    // FIXME - what happens if a thread dies before calling release_semaphore?

    return s;
}

void release_semaphore(semaphore& s) {
    unsigned int release_count = 0;

    // FIXME - what if not held by current thread?

    {
        sync_lock sl;
        lock_guard l1(sl);
        lock_guard l2(s.lock);

        // signal waiting threads

        s.current_count--;

        {
            auto le = thread_list.next;

            while (le != &thread_list) {
                auto& thr = containing_record<thread>(le);

                if (thr.status == thread_status::waiting_object && thr.waiting_for.has_value() && &thr.waiting_for->get() == static_cast<object*>(&s)) {
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

void semaphore::on_wake() {
    lock_guard l(lock);

    current_count++;

    if (current_count == maximum_count)
        signalled = false;
}
