#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"
#include "mem.h"

using namespace std;

extern uint16_t last_tid; // FIXME

static const uint16_t user_CS = 0x3b;
static const uint16_t user_DS = 0x33;

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

    // create section for image

    auto imgh = create_section(file.get(), true, 0);
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

    // create section for stack

    auto stack_size = max(img.stack_size, (uint64_t)PAGE_SIZE);

    auto stackh = create_section(0, false, stack_size);
    if (!stackh) {
        print("create_section failed\n");
        p.release();
        return {};
    }

    auto& stacksect = static_cast<section&>(deref_handle(stackh.get())->get());

    stackh.reset();

    auto stackaddr = p.map_section(stacksect, 0x1000); // FIXME - don't specify address
    if (!stackaddr) {
        print("process::map_section failed\n");
        stacksect.release();
        p.release();
        return {};
    }

    stacksect.release();

    // FIXME - map ntdll into process
    // FIXME - map time page into process

    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return {};
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    // FIXME - start point of thread should actually be in ntdll, so it can do relocations etc.
    if (!p.add_thread(img.preferred_base + img.entry_point, stackaddr + stack_size)) { // FIXME - don't start until process finished initializing
        print("process::add_thread failed\n");
        p.release();
        return {};
    }

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

    if (cr3)
        free_page(cr3);
}

uintptr_t process::map_section(section& sect, uintptr_t address) {
    // FIXME - make address optional, in which case we choose it

    if (address & 0x8000000000000000 || ((address + sect.size) & 0x8000000000000000)) { // don't allow in kernel space
        print("invalid address\n");
        return 0;
    }

    if (address & (PAGE_SIZE - 1)) { // must be on page boundary
        print("invalid address\n");
        return 0;
    }

    if (address & 0xffff000000000000 || (address + sect.size) & 0xffff000000000000) { // VAs limited to 48 bits
        print("invalid address\n");
        return 0;
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
            return m->address;
        }

        if (m->address < m2.address + m2.size && m->address + m->size > m2.address) { // collision
            delete m;
            return 0;
        }

        le = le->next;
    }

    insert_tail_list(mappings, m->le);

    return m->address;
}

bool process::add_thread(uintptr_t entry_point, uintptr_t stack) {
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

    // FIXME - TEB

    memset(&t.context, 0, sizeof(t.context));
    t.context.rip = entry_point;
    t.context.rsp = stack - 0x20; // 0x20 for MS ABI
    t.context.rflags = 0x202; // IF, reserved flag
    t.context.cs = user_CS;
    t.context.ds = user_DS;
    t.context.es = user_DS;
    t.context.fs = user_DS;
    t.context.gs = user_DS;
    t.context.ss = user_DS;

    t.status = thread_status::runnable;

    auto kernel_stack = allocate_stack();
    if (!kernel_stack)
        return false;

    t.syscall_stack = ((uint8_t*)kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT)) - 0x20; // 0x20 for MS ABI

    t.proc = this;
    this->acquire();

    {
        sync_lock sl;
        lock_guard lock(sl);
        insert_tail_list(thread_list, t.le);
    }

    return true;
}

union page_fault_error_code {
    uint64_t value;
    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t user : 1;
        uint64_t reserved_write : 1;
        uint64_t instruction_fetch : 1;
        uint64_t protection_key : 1;
        uint64_t shadow_stack : 1;
        uint64_t reserved1 : 8;
        uint64_t sgx : 1;
        uint64_t reserved2 : 48;
    };
};

static_assert(sizeof(page_fault_error_code) == 8);

bool process::handle_page_fault(uintptr_t address, uint64_t error_code) {
    page_fault_error_code ec;

    ec.value = error_code;

    // FIXME - copy-on-write

    if (ec.present) // NX violation etc.
        return false;

    address &= ~(PAGE_SIZE - 1);

    {
        lock_guard lock(mappings_lock);

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (m.address <= address && m.address + m.size > address)
                return m.sect.map((uint32_t)(address - m.address), address);

            le = le->next;
        }
    }

    return false;
}

bool process::allocate_cr3() {
    cr3 = allocate_page();
    if (!cr3) {
        print("allocate_page failed\n");
        return false;
    }

    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return false;
    }

    if (!map_page(cr3, va, mapping_type::read_write)) {
        print("map_page failed\n");
        free_va(va, 1);
        return false;
    }

    // copy higher half mappings

    memset((void*)va, 0, PAGE_SIZE >> 1);
    memcpy((uint8_t*)va + (PAGE_SIZE >> 1), (uint8_t*)SELFMAP_PML4 + (PAGE_SIZE >> 1), PAGE_SIZE >> 1);

    auto pml4t = (HARDWARE_PTE*)va;

    // add recursive mapping
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pml4t[(SELFMAP & 0xff8000000000) >> 39].PageFrameNumber = cr3 >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pml4t[(SELFMAP & 0xff8000000000) >> 39].Valid = 1;
    pml4t[(SELFMAP & 0xff8000000000) >> 39].Write = 1;

    unmap_page(va);
    free_va(va, 1);

    return true;
}
