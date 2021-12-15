#include "kernel.h"
#include "thread.h"

static uint16_t last_tid = 0; // FIXME - too small?

static const unsigned int KERNEL_STACK_PAGES = 4; // 16KB

static const uint64_t thread_address_block = 0xfffffaf000000000;
static const uint64_t thread_stack_block = 0xfffffa1000000000;

list_entry thread_list = { &thread_list, &thread_list };

amd64_context thread_context;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static constexpr uint64_t tid_to_thread_va(uint16_t tid) {
    return thread_address_block | (tid << 20);
}

static void insert_tail_list(list_entry& head, list_entry& entry) {
    auto* last_tail = head.prev;

    entry.next = &head;
    entry.prev = last_tail;
    last_tail->next = &entry;
    head.prev = &entry;
}

bool add_kernel_thread(void (*func)(void*), void* param) {
    uint16_t tid;

    // FIXME - spinlock for last_tid (or use atomics)
    if (last_tid == 0xffff) { // FIXME - check for lowest unused number
        print("Thread limit reached.\n");
        return false;
    }

    last_tid++;
    tid = last_tid;

    // FIXME - use malloc instead?

    for (unsigned int i = 0; i < bytes_to_pages(sizeof(thread)); i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");
            // FIXME - free other pages
            return false;
        }

        if (!map_page(pa, tid_to_thread_va(tid) + (i << PAGE_SHIFT))) {
            print("map_page failed\n");
            // FIXME - free other pages
            return false;
        }
    }

    auto& t = *(thread*)(uintptr_t)tid_to_thread_va(tid);

    memset(&t, 0, sizeof(thread));
    t.tid = tid;
    t.stack = (void*)(thread_stack_block | (tid << 20));

    for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");
            // FIXME - free other pages
            return false;
        }

        if (!map_page(pa, (uintptr_t)t.stack + (i << PAGE_SHIFT))) {
            print("map_page failed\n");
            // FIXME - free other pages
            return false;
        }
    }

    t.context.rip = (uintptr_t)func;
    t.context.rcx = (uintptr_t)param;
    t.context.rsp = (uintptr_t)((uint8_t*)t.stack + (KERNEL_STACK_PAGES << PAGE_SHIFT) - 0x20); // 0x20 because of MS ABI
    // FIXME - RFLAGS
    // FIXME - segments

    t.status = thread_status::runnable;

    // FIXME - need spinlock
    insert_tail_list(thread_list, t.le);

    return true;
}
