module;

#include "kernel.h"
#include "cpu_block.h"
import objects;
#include "thread.h"
#include "rangelock.h"

export module rangelock;

import bsod;

export struct rangelock_req {
    rangelock_req(rangelock& rl, uint64_t start, uint64_t length) : rl(rl) {
        this->start = start;
        this->length = length;
    }

    void lock();
    void unlock();

    list_entry le;
    rangelock& rl;
    uint64_t start;
    uint64_t length;
    thread* t;
};

static void inline disable_interrupts() {
    __asm __volatile("cli");
}

static void inline enable_interrupts() {
    __asm __volatile("sti");
}

static bool rlr_overlap(const rangelock_req& r1, const rangelock_req& r2) {
    if (r1.start >= r2.start && r1.start < r2.start + r2.length)
        return true;

    if (r1.start + r1.length > r2.start && r1.start + r1.length <= r2.start + r2.length)
        return true;

    if (r1.start < r2.start && r1.start + r1.length > r2.start + r2.length)
        return true;

    return false;
}

void rangelock_req::lock() {
    disable_interrupts();
    acquire_sync_lock();

    bool overlaps = false;

    for (auto le = rl.active.next; le != &rl.active; le = le->next) {
        auto& rlr = containing_record<rangelock_req>(le);

        if (rlr_overlap(rlr, *this)) {
            overlaps = true;
            break;
        }
    }

    if (!overlaps) {
        insert_tail_list(rl.active, le);
        release_sync_lock();
        enable_interrupts();
        return;
    }

    t = get_cpu_block()->current_thread;

    insert_tail_list(rl.waiting, le);

    t->status = thread_status::waiting;
    t->current_cpu = 0xffffffff;
    t->wait_interruptible = false;
    t->wait_timeout = 0;
    yield();
}

void rangelock_req::unlock() {
    disable_interrupts();
    acquire_sync_lock();

    remove_entry_list(le);

    list_entry waking = { &waking, &waking };
    list_entry* next;

    auto le = rl.waiting.next;
    while (le != &rl.waiting) {
        auto& rlr = containing_record<rangelock_req>(le);

        next = le->next;

        bool overlaps = rlr_overlap(rlr, *this);

        if (!overlaps) {
            for (auto le2 = waking.next; le2 != &waking; le2 = le2->next) {
                auto& rlr2 = containing_record<rangelock_req>(le2);

                if (rlr_overlap(rlr, rlr2)) {
                    overlaps = true;
                    break;
                }
            }
        }

        if (!overlaps) {
            remove_entry_list(rlr.le);
            insert_tail_list(waking, rlr.le);
        }

        le = next;
    }

    while (!is_list_empty(waking)) {
        auto& rlr = containing_record<rangelock_req>(waking.next);

        remove_entry_list(rlr.le);
        insert_tail_list(rl.active, rlr.le);

        assert(rlr.t->status == thread_status::waiting);
        rlr.t->status = thread_status::runnable;
    }

    release_sync_lock();
    enable_interrupts();
}
