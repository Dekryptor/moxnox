#include "kernel.h"
#include "rwsem.h"
#include "cpu_block.h"
#include "objects.h"
#include "thread.h"

static void inline disable_interrupts() {
    __asm __volatile("cli");
}

static void inline enable_interrupts() {
    __asm __volatile("sti");
}

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

    insert_tail_list(wait_list_exclusive, t.wait_le);
    t.status = thread_status::waiting_other;
    t.current_cpu = 0xffffffff;
    t.wait_interruptible = false;
    yield();
}

static void wake_up_exclusive_thread(rwsem& sem) {
    auto& t = *(thread*)((uint8_t*)sem.wait_list_exclusive.next - offsetof(thread, wait_le));

    assert(t.status == thread_status::waiting_other);

    remove_entry_list(t.wait_le);
    t.status = thread_status::runnable;
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

    while (!is_list_empty(wait_list_shared)) {
        auto& t = *(thread*)((uint8_t*)wait_list_shared.next - offsetof(thread, wait_le));

        assert(t.status == thread_status::waiting_other);

        remove_entry_list(t.wait_le);
        t.status = thread_status::runnable;
    }

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

    insert_tail_list(wait_list_shared, t.wait_le);
    t.status = thread_status::waiting_other;
    t.current_cpu = 0xffffffff;
    t.wait_interruptible = false;
    yield();
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
