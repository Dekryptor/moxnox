module;

#include "timer-asm.h"
#include <span>
#include <optional>
#include "scheduler.h"
#include "assert.h"

export module rwsem;

import bsod;
import list;
import disable_interrupts;
import cpu_block;
import thread;

export class rwsem {
public:
    void lock();
    void unlock();
    void lock_shared();
    void unlock_shared();

    unsigned int shared_count = 0;
    bool exclusive = false;
    list_entry wait_list_shared = { &wait_list_shared, &wait_list_shared };
    list_entry wait_list_exclusive = { &wait_list_exclusive, &wait_list_exclusive };
};

void rwsem::lock() { // exclusive
    // FIXME - assert that interrupts enabled and cr8 == 0

    disable_interrupts();
    acquire_sync_lock();

    if (!exclusive && shared_count == 0) {
        exclusive = true;
        release_sync_lock();
        enable_interrupts();
        return;
    }

    auto& t = *get_cpu_block()->current_thread;

    insert_tail_list(wait_list_exclusive, t.wait_obj.wait_le);

    set_current_thread_waiting();
}

static void wake_up_exclusive_thread(rwsem& sem) {
    wake_up_waiters(sem.wait_list_exclusive);
}

void rwsem::unlock() { // exclusive
    // FIXME - assert that interrupts enabled and cr8 == 0

    disable_interrupts();
    acquire_sync_lock();

    assert(exclusive);

    if (!is_list_empty(wait_list_exclusive)) {
        wake_up_exclusive_thread(*this);
        release_sync_lock();
        enable_interrupts();
        return;
    }

    exclusive = false;

    wake_up_waiters(wait_list_shared);

    release_sync_lock();
    enable_interrupts();
}

void rwsem::lock_shared() {
    disable_interrupts();
    acquire_sync_lock();

    // FIXME - follow ERESOURCE by not allowing if there's exclusive waiters?

    if (!exclusive) {
        shared_count++;
        release_sync_lock();
        enable_interrupts();
        return;
    }

    auto& t = *get_cpu_block()->current_thread;

    insert_tail_list(wait_list_shared, t.wait_obj.wait_le);

    set_current_thread_waiting();
}

void rwsem::unlock_shared() {
    disable_interrupts();
    acquire_sync_lock();

    assert(!exclusive);

    shared_count--;

    if (shared_count == 0 && !is_list_empty(wait_list_exclusive)) {
        exclusive = true;
        wake_up_exclusive_thread(*this);
    }

    release_sync_lock();
    enable_interrupts();
}
