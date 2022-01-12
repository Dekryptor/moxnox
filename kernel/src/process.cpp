#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"
#include "mem.h"

using namespace std;

extern uint16_t last_tid; // FIXME

static const uint16_t user_CS = 0x3b;
static const uint16_t user_DS = 0x33;

static section* ntdll = nullptr;
static uintptr_t mn_start_process;

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

    auto img_base = p.map_section(img, img.preferred_base);
    if (!img_base) {
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

    auto ntdll_addr = p.map_section(*ntdll, 0x400000); // FIXME - don't specify address
    if (!ntdll_addr) {
        print("process::map_section failed\n");
        p.release();
        return {};
    }

    // FIXME - map time page into process

    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return {};
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    if (!p.add_thread(ntdll_addr + mn_start_process, img_base, ntdll_addr, stackaddr + stack_size)) { // FIXME - don't start until process finished initializing
        print("process::add_thread failed\n");
        p.release();
        return {};
    }

    p.suspend_count = 1;

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
            print("collision\n");
            delete m;
            return 0;
        }

        le = le->next;
    }

    insert_tail_list(mappings, m->le);

    return m->address;
}

bool process::add_thread(uintptr_t entry_point, uintptr_t param1, uintptr_t param2, uintptr_t stack) {
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
    t.context.rcx = param1;
    t.context.rdx = param2;
    t.context.rsp = stack - 0x20; // 0x20 for MS ABI
    t.context.rflags = 0x202; // IF, reserved flag
    t.context.cs = user_CS;
    t.context.ds = user_DS;
    t.context.es = user_DS;
    t.context.fs = user_DS;
    t.context.gs = user_DS;
    t.context.ss = user_DS;
    t.kernel_mode = false;

    t.suspend_count = 1;
    t.status = thread_status::runnable;

    auto kernel_stack = allocate_stack();
    if (!kernel_stack)
        return false;

    t.syscall_stack = ((uint8_t*)kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT)) - 0x20; // 0x20 for MS ABI

    t.proc = this;
    this->acquire();

    {
        lock_guard lock(threads_lock);
        insert_tail_list(this->threads, t.process_le);
    }

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

bool process::copy_from(void* ks_addr, const void* us_addr, size_t size) {
    while (true) {
        lock_guard lock(mappings_lock);
        bool found = false;

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (m.address <= (uintptr_t)us_addr && m.address + m.size > (uintptr_t)us_addr) {
                auto to_copy = min(size, m.address + m.size - (uintptr_t)us_addr);

                if (!m.sect.copy_from((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                    return false;

                if (size == to_copy)
                    return true;

                us_addr = (uint8_t*)us_addr + to_copy;
                ks_addr = (uint8_t*)ks_addr + to_copy;
                size -= to_copy;
                found = true;

                break;
            }

            le = le->next;
        }

        if (!found)
            return false;
    }

    return false;
}

bool process::copy_to(void* us_addr, const void* ks_addr, size_t size) {
    while (true) {
        lock_guard lock(mappings_lock);
        bool found = false;

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (m.address <= (uintptr_t)us_addr && m.address + m.size > (uintptr_t)us_addr) {
                auto to_copy = min(size, m.address + m.size - (uintptr_t)us_addr);

                if (!m.sect.copy_to((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                    return false;

                if (size == to_copy)
                    return true;

                us_addr = (uint8_t*)us_addr + to_copy;
                ks_addr = (uint8_t*)ks_addr + to_copy;
                size -= to_copy;
                found = true;

                break;
            }

            le = le->next;
        }

        if (!found)
            return false;
    }

    return false;
}

void load_ntdll() {
    auto file = open_file("/ntdll.dll");
    if (!file) {
        bsod("open_file failed for ntdll.dll");
        return;
    }

    auto ntdllh = create_section(file.get(), true, 0);
    if (!ntdllh) {
        bsod("create_section failed for ntdll.dll");
        return;
    }

    ntdll = static_cast<section*>(&deref_handle(ntdllh.get())->get());

    ntdllh.reset();

    mn_start_process = ntdll->find_export("mn_start_process");
    if (!mn_start_process) {
        bsod("Could not find export mn_start_process in ntdll.dll");
        return;
    }
}

void process::terminate() {
    unsigned int release_count = 0;

    {
        sync_lock sl;
        lock_guard lock(sl);

        auto le = thread_list.next;

        while (le != &thread_list) {
            auto& t = containing_record<thread>(le);

            if (t.status == thread_status::waiting && t.waiting_for.has_value() && &t.waiting_for->get() == static_cast<object*>(this)) {
                t.status = thread_status::runnable;
                t.waiting_for = nullopt;
                release_count++;
            }

            le = le->next;
        }
    }

    for (unsigned int i = 0; i < release_count; i++) {
        release();
    }
}

NTSTATUS NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status) {
    process* p;
    bool this_process;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        this_process = true;
    } else {
        auto objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw->get();

        if (obj.type != object_type::process) {
            visit_obj(obj, [](auto&& o) {
                o.release();
            });

            return nt_status::OBJECT_TYPE_MISMATCH;
        }

        // FIXME - check access rights

        p = static_cast<process*>(&objrw->get());

        this_process = get_current_thread()->proc;
    }

    if (!this_process) {
        // FIXME
        print("FIXME - terminating other processes\n");
        p->release();
        return nt_status::NOT_IMPLEMENTED;
    }

    // FIXME - kill other threads

    auto t = get_current_thread();

    t->exit_status = exit_status;

    t->release();
    p->release();
    t->terminate(); // doesn't return

    return nt_status::SUCCESS;
}

void process::resume() {
    if (--suspend_count != 0)
        return;

    sync_lock sl;

    lock_guard lock1(sl);
    lock_guard lock2(threads_lock);

    auto le = threads.next;

    while (le != &threads) {
        auto& t = *(thread*)((uint8_t*)le - offsetof(thread, process_le));

        t.suspend_count--;

        le = le->next;
    }
}

NTSTATUS NtResumeProcess(HANDLE proc_handle) {
    process* p;

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();
    } else {
        auto objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw->get();

        if (obj.type != object_type::process) {
            visit_obj(obj, [](auto&& o) {
                o.release();
            });

            return nt_status::OBJECT_TYPE_MISMATCH;
        }

        // FIXME - check access rights

        p = static_cast<process*>(&objrw->get());
    }

    p->resume();
    p->release();

    return nt_status::SUCCESS;
}
