#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"
#include "mem.h"
#include <limits>

using namespace std;

extern uint16_t last_tid; // FIXME

static const uint16_t user_CS = 0x3b;
static const uint16_t user_DS = 0x33;

static const uint64_t MAX_USERSPACE_ADDRESS = 0xffffffffffff;

static section* ntdll = nullptr;
static uintptr_t mn_start_process;

static_assert(sizeof(TEB) == 0x1818);
static_assert(sizeof(PEB) == 0x388);
static_assert(sizeof(RTL_USER_PROCESS_PARAMETERS) == 0x440);

// FIXME - handle thread crashes
// FIXME - 32-bit processes

unique_handle create_process(string_view fn, optional<reference_wrapper<object>> stdinobj,
                             optional<reference_wrapper<object>> stdoutobj,
                             optional<reference_wrapper<object>> stderrobj) {
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

    // create stack

    size_t stack_size = max(img.stack_size, (uint64_t)PAGE_SIZE);
    void* stackaddr = nullptr;

    if (!success(p.allocate_vm(&stackaddr, 0, &stack_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        print("process::map_section failed\n");
        p.release();
        return {};
    }

    auto ntdll_addr = p.map_section(*ntdll, 0x400000); // FIXME - don't specify address
    if (!ntdll_addr) {
        print("process::map_section failed\n");
        p.release();
        return {};
    }

    // FIXME - map time page into process

    handle_t stdinh = 0, stdouth = 0, stderrh = 0;

    if (stdinobj)
        stdinh = p.add_handle(stdinobj->get(), 0); // FIXME - access mask

    if (stdoutobj)
        stdouth = p.add_handle(stdoutobj->get(), 0); // FIXME - access mask

    if (stderrobj)
        stderrh = p.add_handle(stderrobj->get(), 0); // FIXME - access mask

    if (!p.allocate_peb(img_base, stdinh, stdouth, stderrh)) {
        print("process::allocate_peb failed\n");
        p.release();
        return {};
    }

    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return {};
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    if (!p.add_thread(ntdll_addr + mn_start_process, img_base, ntdll_addr, (uintptr_t)stackaddr + stack_size)) {
        print("process::add_thread failed\n");
        p.release();
        return {};
    }

    p.suspend_count = 1;

    auto hv = add_kernel_handle(p, 0); // FIXME - access mask

    p.release(); // add_kernel_handle increases refcount

    return unique_handle{hv};
}

mapping::mapping(section& sect) : type(proc_map_type::section), sect(&sect) {
    sect.acquire();
}

mapping::~mapping() {
    if (sect)
        sect->release();
}

process::~process() {
    free_mappings();

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

bool process::map_to_kernel_space(void* us, void* ks, unsigned int num_pages) {
    auto orig_ks = ks;
    lock_guard lock(mappings_lock);

    auto le = mappings.next;
    while (le != &mappings) {
        auto& m = containing_record<mapping>(le);

        if (m.address <= (uintptr_t)us && m.address + m.size > (uintptr_t)us) {
            auto offset_pages = (uint32_t)(((uintptr_t)us - m.address) >> PAGE_SHIFT);
            auto map_pages = m.size >> PAGE_SHIFT;

            while (offset_pages < map_pages && num_pages > 0) {
                bool succeeded;

                switch (m.type) {
                    case proc_map_type::section:
                        print("FIXME process::map_to_kernel_space: work with section mappings\n"); // FIXME
                        succeeded = false;
                        break;

                    case proc_map_type::reserved:
                        succeeded = false;
                        break;

                    case proc_map_type::committed:
                        succeeded = static_cast<mapping_committed&>(m).map(offset_pages << PAGE_SHIFT, (uint64_t)ks, mapping_type::read_write);
                        if (!succeeded)
                            print("mapping_committed::map failed\n");
                        break;
                }

                if (!succeeded)
                    goto fail;

                offset_pages++;
                us = (uint8_t*)us + PAGE_SIZE;
                ks = (uint8_t*)ks + PAGE_SIZE;
                num_pages--;
            }

            if (num_pages == 0)
                return true;
        } else if ((uint8_t*)m.address > (uint8_t*)us + (num_pages << PAGE_SHIFT))
            break;

        le = le->next;
    }

fail:
    while (orig_ks < ks) {
        unmap_page((uintptr_t)orig_ks);
        orig_ks = (uint8_t*)orig_ks + PAGE_SIZE;
    }

    return false;
}

bool process::populate_teb(thread& t) {
    static const auto num_pages = bytes_to_pages(sizeof(TEB));

    auto va = allocate_va(num_pages);
    if (!va) {
        print("allocate_va failed\n");
        return false;
    }

    if (!map_to_kernel_space(t.teb, (void*)va, num_pages)) {
        print("map_to_kernel_space failed\n");
        free_va(va, num_pages);
        return false;
    }

    auto& teb = *(TEB*)va;

    teb.NtTib.Self = &t.teb->NtTib;
    // FIXME - StackBase
    // FIXME - StackLimit
    // FIXME - ClientId
    teb.ProcessEnvironmentBlock = peb;

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, num_pages);

    return true;
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

    t.teb = nullptr;
    size_t teb_size = sizeof(TEB);

    if (!success(allocate_vm((void**)&t.teb, 0, &teb_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        print("process::allocate_vm failed\n");
        delete &t;
        return false;
    }

    if (!populate_teb(t)) {
        print("populate_teb failed\n");
        delete &t;
        return false;
    }

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

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack)
        return false;

    t.syscall_stack = ((uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT)) - 0x20; // 0x20 for MS ABI

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

            if (m.address <= address && m.address + m.size > address) {
                switch (m.type) {
                    case proc_map_type::section:
                        return m.sect->map((uint32_t)(address - m.address), address);

                    case proc_map_type::reserved:
                        return false;

                    case proc_map_type::committed:
                        return static_cast<mapping_committed&>(m).map((uint32_t)(address - m.address), address, nullopt);
                }
            }

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

                switch (m.type) {
                    case proc_map_type::section:
                        if (!m.sect->copy_from((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                            return false;
                        break;

                    case proc_map_type::reserved:
                        return false;

                    case proc_map_type::committed:
                        if (!static_cast<mapping_committed&>(m).copy_from((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                            return false;
                        break;
                }

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

                switch (m.type) {
                    case proc_map_type::section:
                        if (!m.sect->copy_to((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                            return false;
                        break;

                    case proc_map_type::reserved:
                        return false;

                    case proc_map_type::committed:
                        if (!static_cast<mapping_committed&>(m).copy_to((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                            return false;
                        break;
                }

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

    // wake up threads waiting for process to terminate

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

    // close open handles

    lock_guard lock(handle_lock);
    auto handles = span(handle_arr, no_handles);

    for (auto& ho : handles) {
        auto obj = ho.obj;

        if (!obj)
            continue;

        handle_closed(*obj);

        visit_obj(*obj, [](auto&& o) {
            o.release();
        });
    }

    free_mappings();
}

void process::free_mappings() {
    while (!is_list_empty(mappings)) {
        auto& m = containing_record<mapping>(mappings.next);

        remove_entry_list(m.le);

        if (m.type == proc_map_type::committed) {
            auto& mc = static_cast<mapping_committed&>(m);

            auto num_pages = mc.size >> PAGE_SHIFT;

            for (unsigned int i = 0; i < num_pages; i++) {
                if (mc.pages[i].pa)
                    free_page(mc.pages[i].pa);
            }

            delete &mc;
        } else
            delete &m;
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

handle_t process::add_handle(object& obj, uint32_t access_mask) {
    handle_t h;

    // FIXME - reusing handles?

    {
        lock_guard lock(handle_lock);

        auto needed_pages = bytes_to_pages((no_handles + 1) * sizeof(handle_obj));

        if (needed_pages > handle_arr_pages) {
            auto pa = allocate_page();
            if (!pa) {
                // FIXME - should be exception
                print("allocate_page failed\n");
                return 0;
            }

            auto va = allocate_va(needed_pages);
            if (!va) {
                print("allocate_page failed\n");
                free_page(pa);
                return 0;
            }

            if (handle_arr) {
                // remap pages

                for (unsigned int i = 0; i < handle_arr_pages; i++) {
                    auto pa = va_to_pa((uintptr_t)handle_arr + (i << PAGE_SHIFT));

                    if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
                        print("map_page failed\n");

                        for (unsigned int j = 0; j < i; j++) {
                            unmap_page(va + (j << PAGE_SHIFT));
                        }

                        free_va(va, needed_pages);

                        return 0;
                    }
                }
            }

            if (!map_page(pa, va + (handle_arr_pages << PAGE_SHIFT), mapping_type::read_write)) {
                // FIXME - exception
                print("map_page failed\n");
                free_page(pa);

                for (unsigned int i = 0; i < handle_arr_pages; i++) {
                    unmap_page(va + (i << PAGE_SHIFT));
                }

                free_va(va, needed_pages);

                return 0;
            }

            for (unsigned int i = 0; i < handle_arr_pages; i++) {
                unmap_page((uintptr_t)handle_arr + (i << PAGE_SHIFT));
            }

            if (handle_arr)
                free_va((uintptr_t)handle_arr, handle_arr_pages);

            handle_arr = (handle_obj*)va;
            handle_arr_pages++;
        }

        no_handles++;

        // FIXME - handles should be multiples of 4

        h = no_handles;

        auto& ho = handle_arr[h - 1];
        ho.obj = &obj;
        ho.access_mask = access_mask;
    }

    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });

    obj.handle_count++;

    return h;
}

NTSTATUS process::allocate_vm(void** base_address, uintptr_t, size_t* region_size,
                              uint32_t allocation_type, enum mapping_type protect) {
    // FIXME - MEM_RESERVE alone
    // FIXME - MEM_COMMIT alone

    // FIXME - what are we supposed to do with zero_bits?

    if (!(allocation_type & (MEM_RESERVE | MEM_COMMIT | MEM_RESET)))
        return nt_status::INVALID_PAGE_PROTECTION;

    if (allocation_type & MEM_RESET) {
        print("FIXME: process::allocate_vm MEM_RESET\n"); // FIXME
        return nt_status::INVALID_PARAMETER;
    }

    if (allocation_type & MEM_RESERVE && !(allocation_type & MEM_COMMIT)) {
        print("FIXME: process::allocate_vm MEM_RESERVE\n"); // FIXME
        return nt_status::INVALID_PARAMETER;
    }

    if (allocation_type & MEM_COMMIT && !(allocation_type & MEM_RESERVE)) {
        print("FIXME: process::allocate_vm MEM_COMMIT\n"); // FIXME
        return nt_status::INVALID_PARAMETER;
    }

    auto addr = (uintptr_t)*base_address;
    auto size = *region_size;

    auto end = addr + size;

    // FIXME - if address is not 0 but gets rounded down to 0, do we still choose value ourselves?

    addr &= ~(PAGE_SIZE - 1); // round down

    auto num_pages = bytes_to_pages(end - addr);
    size = num_pages << PAGE_SHIFT;

    if (size == 0)
        return nt_status::INVALID_PARAMETER;

    if (size > numeric_limits<uint32_t>::max())
        return nt_status::INVALID_PARAMETER;

    auto pm = malloc(offsetof(mapping_committed, pages) + (sizeof(page_info) * num_pages));
    if (!pm)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto& m = *new (pm) mapping_committed;

    m.type = proc_map_type::committed;
    m.size = (uint32_t)size;

    auto pages = span(m.pages, num_pages);

    for (auto& p : pages) {
        p.pa = 0;
        p.protect = protect;
    }

    // FIXME - allocate array of pages

    uintptr_t last_addr = 1 << PAGE_SHIFT; // don't allocate VM address 0
    lock_guard lock(mappings_lock);

    // FIXME - specifying address
    // FIXME - protection type

    auto le = mappings.next;
    while (le != &mappings) {
        auto& m2 = containing_record<mapping>(le);

        if (m2.address > last_addr + m.size) { // insert before
            insert_head_list(*le->prev, m.le);
            m.address = last_addr;
            *base_address = (void*)m.address;
            *region_size = m.size;
            return nt_status::SUCCESS;
        }

        // FIXME - if specifying address, handle collisions

        last_addr = m2.address + m2.size;

        le = le->next;
    }

    if (last_addr < MAX_USERSPACE_ADDRESS - m.size) {
        insert_tail_list(mappings, m.le);
        m.address = last_addr;
        *base_address = (void*)m.address;
        *region_size = m.size;
        return nt_status::SUCCESS;
    }

    delete &m;

    return nt_status::NO_MEMORY;
}

NTSTATUS NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                 size_t* region_size, uint32_t allocation_type, uint32_t protect) {
    process* p;
    enum mapping_type prot;

    switch (protect) {
        // FIXME - PAGE_NOACCESS
        // FIXME - PAGE_GUARD

        case PAGE_READONLY:
            prot = mapping_type::read_only;
            break;

        case PAGE_READWRITE:
            prot = mapping_type::read_write;
            break;

        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
            prot = mapping_type::read_execute;
            break;

        case PAGE_NOCACHE:
            prot = mapping_type::no_cache;
            break;

        case PAGE_WRITECOMBINE:
            prot = mapping_type::write_combine;
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

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

    auto ret = p->allocate_vm(base_address, zero_bits, region_size, allocation_type, prot);

    p->release();

    return ret;
}

void process::unmap_proc_page(uint64_t va) {
    print("process::unmap_proc_page: FIXME\n");
    // FIXME
}

NTSTATUS process::free_vm(void** base_address, size_t* region_size, uint32_t free_type) {
    if (!(free_type & (MEM_DECOMMIT | MEM_RELEASE)))
        return nt_status::INVALID_PARAMETER;

    if (free_type & MEM_DECOMMIT) {
        print("FIXME: process::free_vm MEM_DECOMMIT\n"); // FIXME
        return nt_status::INVALID_PARAMETER;
    }

    if (*region_size != 0)
        return nt_status::INVALID_PARAMETER;

    // FIXME - what happens if this is called for section mapping?

    mapping* m = nullptr;

    {
        lock_guard lock(mappings_lock);

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m2 = containing_record<mapping>(le);

            if (m2.address == (uintptr_t)*base_address) {
                // FIXME - MEM_RELEASE with reserved memory block
                if (m2.type == proc_map_type::committed) {
                    auto& mc = static_cast<mapping_committed&>(m2);

                    auto num_pages = mc.size >> PAGE_SHIFT;

                    for (unsigned int i = 0; i < num_pages; i++) {
                        if (mc.pages[i].pa)
                            unmap_proc_page(m2.address + (i << PAGE_SHIFT));
                    }
                } else
                    return nt_status::INVALID_PARAMETER;

                remove_entry_list(m2.le);
                m = &m2;
                break;
            } else if (m2.address > (uintptr_t)*base_address)
                break;

            le = le->next;
        }
    }

    if (!m)
        return nt_status::INVALID_PARAMETER;

    // FIXME - if current process running on other cores and mapping changed, send IPI to clear TLB

    *base_address = (void*)m->address;
    *region_size = m->size;

    if (m->type == proc_map_type::committed) {
        auto& mc = *static_cast<mapping_committed*>(m);

        auto num_pages = mc.size >> PAGE_SHIFT;

        for (unsigned int i = 0; i < num_pages; i++) {
            if (mc.pages[i].pa)
                free_page(mc.pages[i].pa);
        }

        delete &mc;
    } else
        delete m;

    return nt_status::SUCCESS;
}

NTSTATUS NtFreeVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                             uint32_t free_type) {
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

    auto ret = p->free_vm(base_address, region_size, free_type);

    p->release();

    return ret;
}

bool mapping_committed::map(uint32_t offset, uint64_t va, optional<enum mapping_type> prot) {
    bool new_page = false;

    auto& p = pages[offset >> PAGE_SHIFT];

    lock_guard l(lock);

    if (!p.pa) {
        p.pa = allocate_page();
        if (!p.pa) {
            print("allocate_page failed\n");
            return false;
        }

        new_page = true;
    }

    if (!map_page(p.pa, va, prot ? *prot : p.protect))
        return false;

    if (new_page) // FIXME - allocate zeroed page?
        memset((void*)va, 0, PAGE_SIZE);

    return true;
}

bool mapping_committed::copy_from(uint32_t offset, void* ks_addr, uint32_t size) {
    while (true) {
        auto to_copy = min(size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        {
            lock_guard l(lock);
            auto& p = pages[offset >> PAGE_SHIFT];

            if (!p.pa)
                memset(ks_addr, 0, to_copy);
            else {
                auto va = allocate_va(1);
                if (!va) {
                    print("allocate_va failed\n");
                    return false;
                }

                if (!map_page(p.pa, va, mapping_type::read_only)) {
                    print("map_page failed\n");
                    free_va(va, 1);
                    return false;
                }

                memcpy(ks_addr, (uint8_t*)va + (offset & (PAGE_SIZE - 1)), to_copy);

                unmap_page(va);
                free_va(va, 1);
            }
        }

        if (to_copy == size)
            return true;

        ks_addr = (uint8_t*)ks_addr + to_copy;
        offset += to_copy;
        size -= to_copy;
    }
}

bool mapping_committed::copy_to(uint32_t offset, const void* ks_addr, uint32_t size) {
    while (true) {
        auto to_copy = min(size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        {
            lock_guard l(lock);

            auto& p = pages[offset >> PAGE_SHIFT];

            bool new_page = false;

            switch (p.protect) {
                case mapping_type::read_only:
                case mapping_type::read_execute:
                    return false;

                default:
                    break;
            }

            auto va = allocate_va(1);
            if (!va) {
                print("allocate_va failed\n");
                return false;
            }

            if (!p.pa) {
                p.pa = allocate_page();
                if (!p.pa) {
                    print("allocate_page failed\n");
                    free_va(va, 1);
                    return false;
                }

                new_page = true;
            }

            if (!map_page(p.pa, va, mapping_type::read_write)) {
                print("map_page failed\n");
                free_va(va, 1);
                return false;
            }

            if (new_page)
                memset((void*)va, 0, PAGE_SIZE);

            memcpy((uint8_t*)va + (offset & (PAGE_SIZE - 1)), ks_addr, to_copy);

            unmap_page(va);
            free_va(va, 1);
        }

        if (to_copy == size)
            return true;

        ks_addr = (uint8_t*)ks_addr + to_copy;
        offset += to_copy;
        size -= to_copy;
    }
}

bool process::allocate_proc_params(HANDLE stdinh, HANDLE stdouth, HANDLE stderrh) {
    static const auto num_pages = bytes_to_pages(sizeof(RTL_USER_PROCESS_PARAMETERS));

    // FIXME - also include space for command line, environment, etc.

    size_t size = sizeof(RTL_USER_PROCESS_PARAMETERS);

    if (!success(allocate_vm((void**)&proc_params, 0, &size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        print("process::allocate_vm failed\n");
        return false;
    }

    auto va = allocate_va(num_pages);
    if (!va) {
        print("allocate_va failed\n");
        return false;
    }

    if (!map_to_kernel_space(proc_params, (void*)va, num_pages)) {
        print("map_to_kernel_space failed\n");
        free_va(va, num_pages);
        return false;
    }

    auto& ks_proc_params = *(RTL_USER_PROCESS_PARAMETERS*)va;

    ks_proc_params.StandardInput = stdinh;
    ks_proc_params.StandardOutput = stdouth;
    ks_proc_params.StandardError = stderrh;

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, num_pages);

    return true;
}

bool process::allocate_peb(uintptr_t base_address, HANDLE stdinh, HANDLE stdouth, HANDLE stderrh) {
    static const auto num_pages = bytes_to_pages(sizeof(PEB));

    size_t peb_size = sizeof(PEB);

    if (!allocate_proc_params(stdinh, stdouth, stderrh)) {
        print("process::allocate_proc_params failed\n");
        return false;
    }

    if (!success(allocate_vm((void**)&peb, 0, &peb_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        print("process::allocate_vm failed\n");
        return false;
    }

    auto va = allocate_va(num_pages);
    if (!va) {
        print("allocate_va failed\n");
        return false;
    }

    if (!map_to_kernel_space(peb, (void*)va, num_pages)) {
        print("map_to_kernel_space failed\n");
        free_va(va, num_pages);
        return false;
    }

    auto& ks_peb = *(PEB*)va;

    ks_peb.ImageBaseAddress = (void*)base_address;
    ks_peb.ProcessParameters = proc_params;

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, num_pages);

    return true;
}

optional<reference_wrapper<object>> process::deref_handle(handle_t h) {
    lock_guard lock(handle_lock);

    if (h == 0 || h > no_handles)
        return nullopt;

    auto& ho = handle_arr[h - 1];

    if (!ho.obj)
        return nullopt;

    visit_obj(*ho.obj, [](auto&& o) {
        o.acquire(); // needs to be within spinlock
    });

    return *ho.obj;
}
