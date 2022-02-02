#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"
#include "mem.h"
#include <limits>

using namespace std;

extern uint16_t last_tid; // FIXME

static const uint64_t MAX_USERSPACE_ADDRESS = 0xffffffffffff;

static section* ntdll = nullptr;
static uintptr_t mn_start_process;
uintptr_t mn_signal;

static_assert(sizeof(TEB) == 0x1818);
static_assert(sizeof(PEB) == 0x388);
static_assert(sizeof(RTL_USER_PROCESS_PARAMETERS) == 0x440);

// FIXME - handle thread crashes
// FIXME - 32-bit processes

unique_handle create_process(string_view fn, string_view curdir,
                             optional<reference_wrapper<object>> stdinobj,
                             optional<reference_wrapper<object>> stdoutobj,
                             optional<reference_wrapper<object>> stderrobj) {
    auto img_file = open_inode(fn);
    if (!img_file) {
        print("open_inode failed\n");
        return {};
    }

    auto curdir_inode = open_inode(curdir);
    if (!curdir_inode) {
        print("open_inode failed\n");
        return {};
    }

    auto pptr = new process;
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    // create section for image

    auto imgh = create_section(img_file.get(), true, 0);
    if (!imgh) {
        print("create_section failed\n");
        p.release();
        return {};
    }

    p.img = static_cast<section&>(deref_handle(imgh.get())->get());

    imgh.reset();

    auto& img = p.img->get();

    auto img_base = p.map_section(img, img.preferred_base + img.size > MAX_USERSPACE_ADDRESS ? 0 : img.preferred_base);
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

    // FIXME - if ntdll would have collision, choose another address

    p.ntdll_addr = p.map_section(*ntdll, ntdll->preferred_base);
    if (!p.ntdll_addr) {
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

    auto& curdir_inode_obj = static_cast<inode&>(deref_handle(curdir_inode.get())->get());

    auto curdir_file = new file(curdir_inode_obj);
    if (!curdir_file) {
        print("new failed\n");
        curdir_inode_obj.release();
        p.release();
        return {};
    }

    curdir_inode_obj.release();

    auto curdirh = p.add_handle(*curdir_file, 0); // FIXME - access mask

    curdir_file->release();

    if (!p.allocate_peb(img_base, stdinh, stdouth, stderrh, curdirh, curdir)) {
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

    if (!p.add_thread(p.ntdll_addr + mn_start_process, p.ntdll_addr, (uintptr_t)stackaddr + stack_size)) {
        print("process::add_thread failed\n");
        p.release();
        return {};
    }

    p.suspend_count = 1;

    auto hv = add_kernel_handle(p, 0); // FIXME - access mask

    p.release(); // add_kernel_handle increases refcount

    return unique_handle{hv};
}

mapping_section::mapping_section(section& sect) : sect(sect) {
    type = proc_map_type::section;
    sect.acquire();
}

mapping_section::~mapping_section() {
    auto num_pages = size >> PAGE_SHIFT;

    for (unsigned int i = 0; i < num_pages; i++) {
        if (pages[i].pa)
            free_page(pages[i].pa);
    }

    sect.release();
}

process::~process() {
    free_mappings();

    if (img)
        img->get().release();

    if (cr3)
        free_page(cr3);
}

uintptr_t process::map_section(section& sect, uintptr_t address) {
    // FIXME - offset and mapping size

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

    auto num_pages = sect.size >> PAGE_SHIFT;

    auto pm = malloc(offsetof(mapping_section, pages) + (sizeof(page_info) * num_pages));
    if (!pm) {
        print("out of memory\n");
        return 0;
    }

    auto& m = *new (pm) mapping_section(sect);

    m.address = address;
    m.size = sect.size;

    auto pages = span(m.pages, num_pages);

    for (unsigned int i = 0; i < num_pages; i++) {
        pages[i].pa = 0;
        pages[i].protect = sect.pages[i].protect;
    }

    uintptr_t last_addr = 1 << PAGE_SHIFT; // don't allocate VM address 0

    lock_guard lock(mappings_lock);

    auto le = mappings.next;
    while (le != &mappings) {
        auto& m2 = containing_record<mapping>(le);

        if (address == 0) {
            if (m2.address > last_addr + m.size) { // insert before
                m.address = last_addr;
                insert_head_list(*le->prev, m.le);
                return m.address;
            }

            last_addr = m2.address + m2.size;
        } else {
            if (m2.address > address + m.size) { // insert before
                m.address = address;
                insert_head_list(*le->prev, m.le);
                return m.address;
            }

            if (address < m2.address + m2.size && address + m.size > m2.address) { // collision
                print("collision\n");
                delete &m;
                return 0;
            }
        }

        le = le->next;
    }

    if (address == 0) {
        if (last_addr >= MAX_USERSPACE_ADDRESS - m.size)
            return 0;

        m.address = last_addr;
    } else
        m.address = address;

    insert_tail_list(mappings, m.le);

    return m.address;
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

                    case proc_map_type::vm:
                        succeeded = static_cast<mapping_vm&>(m).map(offset_pages << PAGE_SHIFT, (uint64_t)ks, mapping_type::read_write);
                        if (!succeeded)
                            print("mapping_vm::map failed\n");
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

bool process::add_thread(uintptr_t entry_point, uintptr_t param1, uintptr_t stack) {
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
    t.context.Rip = entry_point;
    t.context.Rcx = param1;
    t.context.Rsp = stack - 0x20; // 0x20 for MS ABI
    t.context.EFlags = 0x202; // IF, reserved flag
    t.context.SegCs = user_CS;
    t.context.SegSs = user_DS;
    t.context.SegEs = user_DS;
    t.context.SegFs = user_DS;
    t.context.SegGs = user_DS;
    t.context.SegSs = user_DS;
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

    if (ec.present && !ec.write) // NX violation etc.
        return false;

    address &= ~(PAGE_SIZE - 1);

    {
        lock_guard lock(mappings_lock);

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (m.address <= address && m.address + m.size > address) {
                switch (m.type) {
                    case proc_map_type::section: {
                        auto& ms = static_cast<mapping_section&>(m);

                        if (ec.present) // COW
                            return ms.cow((uint32_t)(address - m.address), address);
                        else
                            return ms.map((uint32_t)(address - m.address), address);
                    }

                    case proc_map_type::vm:
                        if (ec.present)
                            return false;

                        return static_cast<mapping_vm&>(m).map((uint32_t)(address - m.address), address, nullopt);
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
                        if (!static_cast<mapping_section&>(m).copy_from((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                            return false;
                        break;

                    case proc_map_type::vm:
                        if (!static_cast<mapping_vm&>(m).copy_from((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
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
                        if (!static_cast<mapping_section&>(m).sect.copy_to((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
                            return false;
                        break;

                    case proc_map_type::vm:
                        if (!static_cast<mapping_vm&>(m).copy_to((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
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
    auto file = open_inode("/ntdll.dll");
    if (!file) {
        bsod("open_inode failed for ntdll.dll");
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

    mn_signal = ntdll->find_export("mn_signal");
    if (!mn_signal) {
        bsod("Could not find export mn_signal in ntdll.dll");
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

    if (handle_arr_pages != 0) {
        for (unsigned int i = 0; i < handle_arr_pages; i++) {
            auto pa = va_to_pa((uintptr_t)handle_arr + (i << PAGE_SHIFT));

            unmap_page((uintptr_t)handle_arr + (i << PAGE_SHIFT));
            free_page(pa);
        }

        free_va((uintptr_t)handle_arr, handle_arr_pages);
    }

    free_mappings();
}

void process::free_mappings() {
    while (!is_list_empty(mappings)) {
        auto& m = containing_record<mapping>(mappings.next);

        remove_entry_list(m.le);

        switch (m.type) {
            case proc_map_type::vm: {
                auto& mv = static_cast<mapping_vm&>(m);

                auto num_pages = mv.size >> PAGE_SHIFT;

                for (unsigned int i = 0; i < num_pages; i++) {
                    if (mv.pages[i].pa)
                        free_page(mv.pages[i].pa);
                }

                delete &mv;

                break;
            }

            case proc_map_type::section: {
                auto& ms = static_cast<mapping_section&>(m);

                delete &ms;

                break;
            }

            default:
                delete &m;
        }
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

        this_process = p == get_current_thread()->proc;
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
    // FIXME - MEM_COMMIT alone

    // FIXME - what are we supposed to do with zero_bits?

    if (!(allocation_type & (MEM_RESERVE | MEM_COMMIT | MEM_RESET)))
        return nt_status::INVALID_PAGE_PROTECTION;

    if (allocation_type & MEM_RESET) {
        print("FIXME: process::allocate_vm MEM_RESET\n"); // FIXME
        return nt_status::INVALID_PARAMETER;
    }

    auto addr = (uintptr_t)*base_address;
    auto size = *region_size;

    auto end = addr + size;

    addr &= ~(PAGE_SIZE - 1); // round down

    auto num_pages = bytes_to_pages(end - addr);
    size = num_pages << PAGE_SHIFT;

    if (size == 0)
        return nt_status::INVALID_PARAMETER;

    if (size > numeric_limits<uint32_t>::max())
        return nt_status::INVALID_PARAMETER;

    if (allocation_type & MEM_COMMIT && !(allocation_type & MEM_RESERVE)) {
        if (addr == 0)
            return nt_status::INVALID_PARAMETER;

        *base_address = (void*)addr;
        *region_size = size;

        lock_guard lock(mappings_lock);

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (addr < m.address + m.size && addr + size > m.address) {
                switch (m.type) {
                    case proc_map_type::vm: {
                        auto& mv = static_cast<mapping_vm&>(m);

                        auto to_commit = min(size, m.address + m.size - addr);

                        auto ret = mv.commit((uint32_t)(addr - m.address), (uint32_t)to_commit, protect);
                        if (!success(ret))
                            return ret;

                        if (to_commit == size)
                            return nt_status::SUCCESS;

                        addr += to_commit;
                        size -= to_commit;

                        break;
                    }

                    case proc_map_type::section:
                        return nt_status::NO_MEMORY;
                }
            }

            le = le->next;
        }

        return nt_status::NO_MEMORY;
    }

    auto pm = malloc(offsetof(mapping_vm, pages) + (sizeof(page_info) * num_pages));
    if (!pm)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto& m = *new (pm) mapping_vm;

    m.type = proc_map_type::vm;
    m.size = (uint32_t)size;

    auto pages = span(m.pages, num_pages);

    if (!(allocation_type & MEM_COMMIT))
        protect = mapping_type::reserved;

    for (auto& p : pages) {
        p.pa = 0;
        p.protect = protect;
    }

    uintptr_t last_addr = 1 << PAGE_SHIFT; // don't allocate VM address 0
    lock_guard lock(mappings_lock);

    auto le = mappings.next;
    while (le != &mappings) {
        auto& m2 = containing_record<mapping>(le);

        if (addr == 0) {
            if (m2.address > last_addr + m.size) { // insert before
                insert_head_list(*le->prev, m.le);
                m.address = last_addr;
                *base_address = (void*)m.address;
                *region_size = m.size;
                return nt_status::SUCCESS;
            }

            last_addr = m2.address + m2.size;
        } else {
            if (m2.address > addr + m.size) { // insert before
                insert_head_list(*le->prev, m.le);
                m.address = addr;
                *base_address = (void*)m.address;
                *region_size = m.size;
                return nt_status::SUCCESS;
            }

            if (addr < m2.address + m2.size && addr + m.size > m2.address) { // collision
                print("collision\n");
                delete &m;
                return nt_status::NO_MEMORY;
            }
        }

        le = le->next;
    }

    if (addr == 0) {
        if (last_addr >= MAX_USERSPACE_ADDRESS - m.size) {
            delete &m;
            return nt_status::NO_MEMORY;
        }

        m.address = last_addr;
    } else
        m.address = addr;

    *base_address = (void*)m.address;
    *region_size = m.size;
    insert_tail_list(mappings, m.le);

    return nt_status::SUCCESS;
}

NTSTATUS NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                 size_t* region_size, uint32_t allocation_type, uint32_t protect) {
    process* p;
    enum mapping_type prot;

    switch (protect) {
        case PAGE_NOACCESS:
        case PAGE_GUARD:
            prot = mapping_type::no_access;
            break;

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

        case PAGE_EXECUTE_READWRITE:
            prot = mapping_type::read_write_execute;
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

NTSTATUS process::unmap_proc_page(uint64_t va) {
    NTSTATUS status;
    span<HARDWARE_PTE> pml4, pdpt, pdt, pt;

    auto buf = allocate_va(4);
    if (!buf) {
        print("allocate_va failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!map_page(cr3, buf, mapping_type::read_only)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end;
    }

    pml4 = span((HARDWARE_PTE*)buf, 512);

    if (!pml4[(va & 0xff8000000000) >> 39].Valid) {
        status = nt_status::SUCCESS;
        goto end2;
    }

    if (!map_page((pml4[(va & 0xff8000000000) >> 39].PageFrameNumber) << PAGE_SHIFT, buf + PAGE_SIZE, mapping_type::read_only)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end2;
    }

    pdpt = span((HARDWARE_PTE*)(buf + PAGE_SIZE), 512);

    if (!pdpt[(va & 0x7fc0000000) >> 30].Valid) {
        status = nt_status::SUCCESS;
        goto end3;
    }

    if (!map_page((pdpt[(va & 0x7fc0000000) >> 30].PageFrameNumber) << PAGE_SHIFT, buf + (2 * PAGE_SIZE), mapping_type::read_only)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end3;
    }

    pdt = span((HARDWARE_PTE*)(buf + (2 * PAGE_SIZE)), 512);

    if (!pdt[(va & 0x3fe00000) >> 21].Valid) {
        status = nt_status::SUCCESS;
        goto end4;
    }

    if (!map_page((pdt[(va & 0x3fe00000) >> 21].PageFrameNumber) << PAGE_SHIFT, buf + (3 * PAGE_SIZE), mapping_type::read_write)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end4;
    }

    pt = span((HARDWARE_PTE*)(buf + (3 * PAGE_SIZE)), 512);

    pt[(va & 0x1ff000) >> 12].Valid = 0;

    status = nt_status::SUCCESS;

    unmap_page(buf + (3 * PAGE_SIZE));

end4:
    unmap_page(buf + (2 * PAGE_SIZE));

end3:
    unmap_page(buf + PAGE_SIZE);

end2:
    unmap_page(buf);

end:
    free_va(buf, 4);

    return status;
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

    mapping* pm = nullptr;

    {
        lock_guard lock(mappings_lock);

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m2 = containing_record<mapping>(le);

            if (m2.address == (uintptr_t)*base_address) {
                // FIXME - MEM_RELEASE with reserved memory block
                if (m2.type == proc_map_type::vm) {
                    auto& mv = static_cast<mapping_vm&>(m2);

                    auto num_pages = mv.size >> PAGE_SHIFT;

                    for (unsigned int i = 0; i < num_pages; i++) {
                        if (mv.pages[i].pa) {
                            auto status = unmap_proc_page(m2.address + (i << PAGE_SHIFT));
                            if (!success(status))
                                return status;
                        }
                    }
                } else
                    return nt_status::INVALID_PARAMETER;

                remove_entry_list(m2.le);
                pm = &m2;
                break;
            } else if (m2.address > (uintptr_t)*base_address)
                break;

            le = le->next;
        }
    }

    if (!pm)
        return nt_status::INVALID_PARAMETER;

    auto& m = *pm;

    // FIXME - if current process running on other cores and mapping changed, send IPI to clear TLB

    *base_address = (void*)m.address;
    *region_size = m.size;

    switch (m.type) {
        case proc_map_type::vm: {
            auto& mv = static_cast<mapping_vm&>(m);

            auto num_pages = mv.size >> PAGE_SHIFT;

            for (unsigned int i = 0; i < num_pages; i++) {
                if (mv.pages[i].pa)
                    free_page(mv.pages[i].pa);
            }

            delete &mv;

            break;
        }

        case proc_map_type::section: {
            auto& ms = static_cast<mapping_section&>(m);

            delete &ms;

            break;
        }

        default:
            delete &m;
    }

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

bool mapping_vm::map(uint32_t offset, uint64_t va, optional<enum mapping_type> prot) {
    bool new_page = false;

    auto& p = pages[offset >> PAGE_SHIFT];

    lock_guard l(lock);

    if (p.protect == mapping_type::reserved)
        return false;

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

bool mapping_vm::copy_from(uint32_t offset, void* ks_addr, uint32_t size) {
    while (true) {
        auto to_copy = min(size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        {
            lock_guard l(lock);
            auto& p = pages[offset >> PAGE_SHIFT];

            if (p.protect == mapping_type::reserved)
                return false;

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

bool mapping_section::copy_from(uint32_t offset, void* ks_addr, uint32_t size) {
    while (true) {
        auto to_copy = min(size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        {
            lock_guard l(lock);
            auto& p = pages[offset >> PAGE_SHIFT];

            if (!p.pa) {
                if (!sect.copy_from(offset, ks_addr, (uint32_t)to_copy))
                    return false;
            } else {
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

bool mapping_vm::copy_to(uint32_t offset, const void* ks_addr, uint32_t size) {
    while (true) {
        auto to_copy = min(size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        {
            lock_guard l(lock);

            auto& p = pages[offset >> PAGE_SHIFT];

            bool new_page = false;

            switch (p.protect) {
                case mapping_type::read_only:
                case mapping_type::read_execute:
                case mapping_type::reserved:
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

static size_t dos_path_len(string_view s) {
    // FIXME - resolve paths to DOS paths properly

    auto len = utf8_to_utf16_len(s);

    len += 2; // for "C:"

    return len;
}

static void create_dos_path(span<char16_t> buf, string_view s) {
    // FIXME - resolve paths to DOS paths properly

    if (!buf.empty()) {
        buf[0] = 'C';
        buf = buf.subspan(1);
    }

    if (!buf.empty()) {
        buf[0] = ':';
        buf = buf.subspan(1);
    }

    utf8_to_utf16(buf, s);

    for (auto& c : buf) {
        if (c == '/')
            c = '\\';
    }
}

bool process::allocate_proc_params(HANDLE stdinh, HANDLE stdouth, HANDLE stderrh, HANDLE curdirh,
                                   string_view curdir) {
    // FIXME - also include space for command line, environment, etc.

    auto curdir_len = dos_path_len(curdir);
    size_t size = sizeof(RTL_USER_PROCESS_PARAMETERS) + ((curdir_len + 1) * sizeof(char16_t));

    if (!success(allocate_vm((void**)&proc_params, 0, &size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        print("process::allocate_vm failed\n");
        return false;
    }

    auto num_pages = bytes_to_pages(size);

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
    ks_proc_params.CurrentDirectory.Handle = curdirh;

    ks_proc_params.CurrentDirectory.DosPath.Length = (uint16_t)(curdir_len * sizeof(char16_t));
    ks_proc_params.CurrentDirectory.DosPath.MaximumLength = ks_proc_params.CurrentDirectory.DosPath.Length + sizeof(char16_t);
    ks_proc_params.CurrentDirectory.DosPath.Buffer = (char16_t*)((uint8_t*)proc_params + sizeof(RTL_USER_PROCESS_PARAMETERS));

    auto ks_dos_path = span((char16_t*)(va + sizeof(RTL_USER_PROCESS_PARAMETERS)), ks_proc_params.CurrentDirectory.DosPath.MaximumLength / sizeof(char16_t));
    create_dos_path(ks_dos_path, curdir);

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, num_pages);

    return true;
}

bool process::allocate_peb(uintptr_t base_address, HANDLE stdinh, HANDLE stdouth, HANDLE stderrh,
                           HANDLE curdirh, string_view curdir) {
    static const auto num_pages = bytes_to_pages(sizeof(PEB));

    size_t peb_size = sizeof(PEB);

    if (!allocate_proc_params(stdinh, stdouth, stderrh, curdirh, curdir)) {
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

NTSTATUS process::change_proc_page_protection(uint64_t va, enum mapping_type prot) {
    NTSTATUS status;
    span<HARDWARE_PTE> pml4, pdpt, pdt, pt;

    auto buf = allocate_va(4);
    if (!buf) {
        print("allocate_va failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!map_page(cr3, buf, mapping_type::read_only)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end;
    }

    pml4 = span((HARDWARE_PTE*)buf, 512);

    if (!pml4[(va & 0xff8000000000) >> 39].Valid) {
        status = nt_status::SUCCESS;
        goto end2;
    }

    if (!map_page((pml4[(va & 0xff8000000000) >> 39].PageFrameNumber) << PAGE_SHIFT, buf + PAGE_SIZE, mapping_type::read_only)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end2;
    }

    pdpt = span((HARDWARE_PTE*)(buf + PAGE_SIZE), 512);

    if (!pdpt[(va & 0x7fc0000000) >> 30].Valid) {
        status = nt_status::SUCCESS;
        goto end3;
    }

    if (!map_page((pdpt[(va & 0x7fc0000000) >> 30].PageFrameNumber) << PAGE_SHIFT, buf + (2 * PAGE_SIZE), mapping_type::read_only)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end3;
    }

    pdt = span((HARDWARE_PTE*)(buf + (2 * PAGE_SIZE)), 512);

    if (!pdt[(va & 0x3fe00000) >> 21].Valid) {
        status = nt_status::SUCCESS;
        goto end4;
    }

    if (!map_page((pdt[(va & 0x3fe00000) >> 21].PageFrameNumber) << PAGE_SHIFT, buf + (3 * PAGE_SIZE), mapping_type::read_write)) {
        print("map_page failed\n");
        status = nt_status::INSUFFICIENT_RESOURCES;
        goto end4;
    }

    pt = span((HARDWARE_PTE*)(buf + (3 * PAGE_SIZE)), 512);

    protect_page2(pt[(va & 0x1ff000) >> 12], prot, false);

    status = nt_status::SUCCESS;

    unmap_page(buf + (3 * PAGE_SIZE));

end4:
    unmap_page(buf + (2 * PAGE_SIZE));

end3:
    unmap_page(buf + PAGE_SIZE);

end2:
    unmap_page(buf);

end:
    free_va(buf, 4);

    return status;
}

NTSTATUS process::protect_vm(void** base_address, size_t* region_size, enum mapping_type prot,
                             enum mapping_type& old_prot) {
    auto addr = (uintptr_t)*base_address;
    auto addr_end = addr + *region_size;

    addr &= ~(PAGE_SIZE - 1);

    auto pages = bytes_to_pages(addr_end - addr);

    if (pages == 0)
        return nt_status::INVALID_PARAMETER;

    *base_address = (void*)addr;
    *region_size = pages << PAGE_SHIFT;

    lock_guard lock(mappings_lock);

    auto le = mappings.next;
    while (le != &mappings) {
        auto& m = containing_record<mapping>(le);

        if (m.address > addr + (pages << PAGE_SHIFT))
            return nt_status::NO_MEMORY;

        if (m.address + m.size > addr && m.address < addr + (pages << PAGE_SHIFT)) {
            auto num_pages = min(pages, (uint32_t)((m.address + m.size - addr) >> PAGE_SHIFT));

            switch (m.type) {
                case proc_map_type::section: {
                    auto& ms = static_cast<mapping_section&>(m);

                    // FIXME - mapping might be offset in section
                    // FIXME - should be private to process
                    // FIXME - section might not have been opened read-write, etc.

                    auto ret = ms.protect((uint32_t)(addr - m.address), num_pages, prot, old_prot);
                    if (!success(ret))
                        return ret;

                    auto p = &ms.pages[(addr - m.address) >> PAGE_SHIFT];

                    for (unsigned int i = 0; i < num_pages; i++) {
                        auto prot2 = prot;

                        if (!p->pa) { // COW
                            // FIXME - don't do COW unless we ought to

                            switch (prot) {
                                case mapping_type::read_write:
                                case mapping_type::write_combine:
                                case mapping_type::no_cache:
                                    prot2 = mapping_type::read_only;
                                    break;

                                case mapping_type::read_write_execute:
                                    prot2 = mapping_type::read_execute;
                                    break;

                                default:
                                    break;
                            }
                        }

                        // FIXME - would be quicker if we could do multiple pages at once
                        auto ret = change_proc_page_protection(addr + (i << PAGE_SHIFT), prot2);
                        if (!success(ret))
                            return ret;

                        p++;
                    }

                    break;
                }

                case proc_map_type::vm:
                    print("FIXME - protecting VM\n"); // FIXME
                    return nt_status::INTERNAL_ERROR;
            }

            pages -= num_pages;

            if (pages == 0)
                break;

            addr += num_pages << PAGE_SHIFT;
        }

        le = le->next;
    }

    if (pages != 0)
        return nt_status::NO_MEMORY;

    // FIXME - if mappings changed and process running on other cores, send IPI to clear TLB

    return nt_status::SUCCESS;
}

NTSTATUS NtProtectVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                uint32_t new_prot, uint32_t* old_prot) {
    enum mapping_type prot;
    process* p;

    switch (new_prot) {
        case PAGE_NOACCESS:
        case PAGE_GUARD:
            prot = mapping_type::no_access;
            break;

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

        case PAGE_EXECUTE_READWRITE:
            prot = mapping_type::read_write_execute;
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

    enum mapping_type old_type;

    auto ret = p->protect_vm(base_address, region_size, prot, old_type);

    p->release();

    if (success(ret)) {
        switch (old_type) {
            case mapping_type::no_access:
            case mapping_type::reserved:
                *old_prot = PAGE_NOACCESS;
                break;

            case mapping_type::read_write:
                *old_prot = PAGE_READWRITE;
                break;

            case mapping_type::read_only:
                *old_prot = PAGE_READONLY;
                break;

            case mapping_type::read_execute:
                *old_prot = PAGE_EXECUTE_READ;
                break;

            case mapping_type::read_write_execute:
                *old_prot = PAGE_EXECUTE_READWRITE;
                break;

            case mapping_type::no_cache:
                *old_prot = PAGE_NOCACHE;
                break;

            case mapping_type::write_combine:
                *old_prot = PAGE_WRITECOMBINE;
                break;
        }
    }

    return ret;
}

NTSTATUS mapping_section::protect(uint32_t offset, uint32_t page_count, enum mapping_type prot,
                                  enum mapping_type& old_prot) {
    auto p = &pages[offset >> PAGE_SHIFT];

    for (unsigned int i = 0; i < page_count; i++) {
        if (i == 0)
            old_prot = p->protect;

        p->protect = prot;
        p++;
    }

    return nt_status::SUCCESS;
}

bool mapping_section::map(uint32_t offset, uint64_t va) {
    uint64_t pa;

    {
        lock_guard l(lock);

        pa = pages[offset >> PAGE_SHIFT].pa;
    }

    if (pa) // have local copy of COW'd page
        return map_page(pa, va, pages[offset >> PAGE_SHIFT].protect);

    return sect.map(offset, va, true);
}

bool mapping_section::cow(uint32_t offset, uint64_t va) {
    uint64_t pa;

    {
        lock_guard l(lock);

        switch (pages[offset >> PAGE_SHIFT].protect) {
            case mapping_type::no_access:
            case mapping_type::read_only:
            case mapping_type::read_execute:
                return false;

            default:
                break;
        }

        pa = pages[offset >> PAGE_SHIFT].pa;
    }

    if (pa) // have local copy of COW'd page
        return map_page(pa, va, pages[offset >> PAGE_SHIFT].protect);

    auto va2 = allocate_va(2);
    if (!va2) {
        print("allocate_va failed\n");
        return false;
    }

    if (!sect.map(offset, va2, true)) {
        print("section::map failed\n");
        free_va(va2, 2);
        return false;
    }

    auto pa_new = allocate_page();
    if (!pa_new) {
        print("allocate_page failed\n");
        unmap_page(va2);
        free_va(va2, 2);
        return false;
    }

    if (!map_page(pa_new, va2 + PAGE_SIZE, mapping_type::read_write)) {
        print("map_page failed\n");
        unmap_page(va2);
        free_va(va2, 2);
        free_page(pa_new);
        return false;
    }

    memcpy((void*)(va2 + PAGE_SIZE), (void*)va2, PAGE_SIZE);

    unmap_page(va2);
    unmap_page(va2 + PAGE_SIZE);
    free_va(va2, 2);

    bool beaten_to_it = false;

    {
        lock_guard l(lock);

        pa = pages[offset >> PAGE_SHIFT].pa;

        if (pa)
            beaten_to_it = true;
        else {
            pages[offset >> PAGE_SHIFT].pa = pa_new;
            pa = pa_new;
        }
    }

    if (beaten_to_it)
        free_page(pa_new);

    return map_page(pa, va, pages[offset >> PAGE_SHIFT].protect);
}

NTSTATUS mapping_vm::commit(uint32_t offset, uint32_t commit_size, enum mapping_type prot) {
    auto num_pages = commit_size >> PAGE_SHIFT;

    lock_guard l(lock);

    auto p = &pages[offset >> PAGE_SHIFT];

    for (unsigned int i = 0; i < num_pages; i++) {
        p->protect = prot;
        p++;
    }

    return nt_status::SUCCESS;
}

NTSTATUS process::close_handle(handle_t h) {
    if (!h)
        return nt_status::INVALID_HANDLE;

    object* obj;

    {
        lock_guard lock(handle_lock);

        if (h > no_handles)
            return nt_status::INVALID_HANDLE;

        auto& ho = handle_arr[h - 1];

        if (!ho.obj)
            return nt_status::INVALID_HANDLE;

        obj = ho.obj;
        ho.obj = nullptr;

        if (h == no_handles)
            no_handles--;
    }

    visit_obj(*obj, [](auto&& o) {
        o.release();
    });

    return nt_status::SUCCESS;
}

NTSTATUS process::map_view_of_section(section& sect, void** base_address, uintptr_t, size_t, LARGE_INTEGER*,
                                      size_t& view_size, enum SECTION_INHERIT, uint32_t, uint32_t) {
    // FIXME - zero_bits
    // FIXME - commit_size
    // FIXME - section_offset
    // FIXME - view_size
    // FIXME - inherit_disposition
    // FIXME - allocation_type
    // FIXME - win32_protect

    auto addr = (uintptr_t)*base_address;

    if (addr & (PAGE_SIZE - 1))
        addr &= ~(PAGE_SIZE - 1);

    addr = map_section(sect, addr);

    if (addr == 0)
        return nt_status::INTERNAL_ERROR;

    *base_address = (void*)addr;
    view_size = sect.size; // FIXME

    return nt_status::SUCCESS;
}

NTSTATUS process::unmap_view_of_section(void* base_address) {
    auto addr = (uintptr_t)base_address;

    mapping_section* ms2 = nullptr;

    {
        lock_guard lock(mappings_lock);

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (m.address <= addr && m.address + m.size > addr) {
                if (m.type != proc_map_type::section)
                    return nt_status::INVALID_PARAMETER;

                auto& ms = static_cast<mapping_section&>(m);

                auto num_pages = ms.size >> PAGE_SHIFT;

                for (unsigned int i = 0; i < num_pages; i++) {
                    // FIXME - unmap_proc_page should take range
                    auto status = unmap_proc_page(m.address + (i << PAGE_SHIFT));
                    if (!success(status))
                        return status;
                }

                remove_entry_list(m.le);
                ms2 = &ms;
                break;
            } else if (m.address > addr)
                break;

            le = le->next;
        }
    }

    if (!ms2)
        return nt_status::NO_MEMORY;

    delete ms2;

    return nt_status::SUCCESS;
}
