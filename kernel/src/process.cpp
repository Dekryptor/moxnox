#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"

using namespace std;

extern uint16_t last_tid; // FIXME

static const uint16_t user_CS = 0x8; // FIXME
static const uint16_t user_DS = 0x10; // FIXME

// FIXME - make sure process signalled when it dies
// FIXME - handle thread crashes
// FIXME - kill process when last thread dies
// FIXME - 32-bit processes

unique_handle create_process(string_view fn) {
    auto file = open_file(fn);
    if (!file) {
        print("open_file failed\n");
        return {};
    }

    // FIXME - create section from file
    // FIXME - make sure section refers to PE file

    auto pptr = new process;
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    auto imgh = create_section(file.get(), true);
    if (!imgh) {
        print("create_section failed\n");
        p.release();
        return {};
    }

    p.img = static_cast<section&>(deref_handle(imgh.get())->get());

    imgh.reset();

    auto& img = p.img->get();

    if (!p.map_section(img, img.preferred_base)) {
        print("process::map_section failed\n");
        p.release();
        return {};
    }

    // FIXME - map ntdll into process
    // FIXME - map time page into process

    // FIXME - create user thread for process, with stack
    // FIXME - start point of thread should actually be in ntdll, so it can do relocations etc.
    if (!p.add_thread(img.preferred_base + img.entry_point)) {
        print("process::add_thread failed\n");
        p.release();
        return {};
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    auto hv = add_handle(p, 0); // FIXME - access mask

    p.release(); // add_handle increases refcount

    return unique_handle{hv};
}

mapping::mapping(section& sect) : sect(sect) {
    sect.acquire();
}

mapping::~mapping() {
    sect.release();
}

process::~process() {
    while (!is_list_empty(mappings)) {
        auto& m = containing_record<mapping>(mappings.next);

        remove_entry_list(m.le);
        delete &m;
    }

    if (img)
        img->get().release();
}

bool process::map_section(section& sect, uint64_t address) {
    // FIXME - make address optional, in which case we choose it

    if (address & 0x8000000000000000 || ((address + sect.size) & 0x8000000000000000)) { // don't allow in kernel space
        print("invalid address\n");
        return false;
    }

    if (address & (PAGE_SIZE - 1)) { // must be on page boundary
        print("invalid address\n");
        return false;
    }

    if (address & 0xffff000000000000 || (address + sect.size) & 0xffff000000000000) { // VAs limited to 48 bits
        print("invalid address\n");
        return false;
    }

    auto m = new mapping(sect);
    m->address = address;
    m->size = sect.size;

    lock_guard lock(mappings_lock);

    auto le = mappings.next;
    while (le != &mappings) {
        auto& m2 = containing_record<mapping>(le);

        if (m2.address > m->address + m->size) { // insert before
            insert_head_list(*le->prev, m->le);
            return true;
        }

        if (m->address < m2.address + m2.size && m->address + m->size > m2.address) { // collision
            delete m;
            return false;
        }

        le = le->next;
    }

    insert_tail_list(mappings, m->le);

    return true;
}

bool process::add_thread(uintptr_t entry_point) {
    uint16_t tid;

    auto tptr = new thread;
    if (!tptr) {
        print("new failed\n");
        return false;
    }

    auto& t = *tptr;

    // FIXME - spinlock for last_tid (or use atomics)
    if (last_tid == 0xffff) { // FIXME - check for lowest unused number
        print("Thread limit reached.\n");
        delete &t;
        return false;
    }

    last_tid++;
    tid = last_tid;

    t.tid = tid;

    // FIXME - stack

    memset(&t.context, 0, sizeof(t.context));
    t.context.rip = entry_point;
    // FIXME - rsp
    t.context.rflags = 0x202; // IF, reserved flag
    t.context.cs = user_CS;
    t.context.ds = user_DS;
    t.context.es = user_DS;
    t.context.fs = user_DS;
    t.context.gs = user_DS;
    t.context.ss = user_DS;

    t.status = thread_status::runnable;

    t.proc = this;
    this->acquire();

    {
        lock_guard lock(sync_lock);
        insert_tail_list(thread_list, t.le);
    }

    return true;
}
