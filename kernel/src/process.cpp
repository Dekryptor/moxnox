#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"

using namespace std;

// FIXME - make sure process signalled when it dies

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
        print("map_section failed\n");
        p.release();
        return {};
    }

    // FIXME - map ntdll into process
    // FIXME - map time page into process

    // FIXME - create user thread for process, with stack
    // FIXME - start point of thread should actually be in ntdll, so it can do relocations etc.

    // FIXME - token for process

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
