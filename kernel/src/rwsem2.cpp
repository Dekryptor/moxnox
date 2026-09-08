module;

#include <stdint.h>
#include <stddef.h>
#include "timer-asm.h"
#include "assert.h"

module rwsem;

import bsod;
import disable_interrupts;
import cpu_block;
import scheduler;
import thread;

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

    t.wait_obj.reading = true;
    insert_tail_list(wait_list_exclusive, t.wait_obj.wait_le);

    set_current_thread_waiting(false);
}

bool rwsem::try_lock() { // exclusive
    bool ret;

    // FIXME - assert that interrupts enabled and cr8 == 0

    disable_interrupts();
    acquire_sync_lock();

    if (!exclusive && shared_count == 0) {
        exclusive = true;
        ret = true;
    } else
        ret = false;

    release_sync_lock();
    enable_interrupts();

    return ret;
}

static void wake_up_exclusive_thread(rwsem& sem) {
    list_entry tmp = { &tmp, &tmp };

    assert(!is_list_empty(sem.wait_list_exclusive));

    auto& wo = *(waiter_object*)((uint8_t*)sem.wait_list_exclusive.next - offsetof(waiter_object, wait_le));

    // put top entry on temporary list, so we can use wake_up_waiters

    remove_entry_list(wo.wait_le);
    insert_tail_list(tmp, wo.wait_le);
    wake_up_waiters(tmp, true);

    assert(is_list_empty(tmp));
}

void rwsem::unlock() { // exclusive
    // FIXME - assert that interrupts enabled and cr8 == 0

    disable_interrupts();
    acquire_sync_lock();

    assert(exclusive);

    if (shared_count == 0 && !is_list_empty(wait_list_exclusive)) {
        wake_up_exclusive_thread(*this);
        release_sync_lock();
        enable_interrupts();
        return;
    }

    exclusive = false;

    wake_up_waiters(wait_list_shared, true);

    release_sync_lock();
    enable_interrupts();
}

void rwsem::lock_shared() {
    disable_interrupts();
    acquire_sync_lock();

    assert(shared_count >= 0);

    shared_count++;

    if (!exclusive && is_list_empty(wait_list_exclusive)) {
        release_sync_lock();
        enable_interrupts();
        return;
    }

    auto& t = *get_cpu_block()->current_thread;

    t.wait_obj.reading = true;
    insert_tail_list(wait_list_shared, t.wait_obj.wait_le);

    set_current_thread_waiting(false);
}

void rwsem::unlock_shared() {
    disable_interrupts();
    acquire_sync_lock();

    assert(!exclusive);
    assert(shared_count > 0);

    shared_count--;

    if (shared_count == 0 && !is_list_empty(wait_list_exclusive)) {
        exclusive = true;
        wake_up_exclusive_thread(*this);
    }

    release_sync_lock();
    enable_interrupts();
}
