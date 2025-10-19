module;

#include <nt.h>
#include <span>
#include <functional>
#include <atomic>
#include <optional>
#include <mutex>
#include "scheduler.h"

export module semaphore;

import print;
import list;
import bsod;
import object_ref;
import objects;
import mem;
import spinlock;

using namespace std;

export class semaphore : public sync_object {
public:
    semaphore() : sync_object(object_type::semaphore, false) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void on_wake();

    unsigned int maximum_count;
    unsigned int current_count;
    spinlock lock;
};

static_assert(is_sync_object<semaphore>);
static_assert(object_has_on_wake<semaphore>);

export template<bool no_lock = false>
object_ref create_semaphore(unsigned int maximum_count) {
    semaphore* sptr;

    if constexpr (no_lock) {
        auto ptr = malloc_aligned<true>(sizeof(semaphore), alignof(semaphore));
        if (!ptr) {
            print("malloc_aligned failed\n");
            return {};
        }

        sptr = new (ptr) semaphore;
    } else {
        sptr = new semaphore;
        if (!sptr) {
            print("new failed\n");
            return {};
        }
    }

    auto& s = *sptr;
    s.maximum_count = maximum_count;
    s.current_count = 0;
    s.signalled = true;

    // FIXME - what happens if a thread dies before calling release_semaphore?

    return s;
}

export void release_semaphore(semaphore& s) {
    // FIXME - what if not held by current thread?

    sync_lock sl;
    lock_guard l1(sl);
    lock_guard l2(s.lock);

    // wake up one thread

    s.current_count--;

    auto waiter_count = wake_up_waiters(s.waiters);

    if (waiter_count == 0)
        s.signalled = true;
}

void semaphore::on_wake() {
    lock_guard l(lock);

    current_count++;

    if (current_count == maximum_count)
        signalled = false;
}
