#include <limits>
#include "kernel.h"
import objects;
#include "mem.h"
#include "handle.h"
#include "spinlock.h"
import rbtree;
import event;
import pipe;
import timer;
#include "inode.h"
import volume;
#include "visit_object.h"
#include <pe.h>
#include <ntdll.h>

import print;
import bsod;
import mem;
import config;
import list;
import vfs;
import string;
import interrupts;
import pe;

using namespace std;

extern uint16_t last_tid; // FIXME

static const uint64_t MAX_USERSPACE_ADDRESS = 0xffffffffffff;

section* ntdll = nullptr;

uintptr_t mn_start_thread, mn_signal;

static_assert(sizeof(TEB) == 0x1818);
static_assert(sizeof(PEB) == 0x388);
static_assert(sizeof(RTL_USER_PROCESS_PARAMETERS) == 0x440);

enum class proc_map_type {
    section,
    vm
};

class mapping : public cacheable {
public:
    virtual ~mapping() {
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
    bool copy_to(uint32_t offset, const void* ks_addr, uint32_t size);

    list_entry le;
    enum proc_map_type type;
    uint64_t address;
    uint32_t size;

private:
    atomic<unsigned int> refcount = 1;
};

class mapping_section : public mapping {
public:
    mapping_section(section& sect) : sect(sect) {
        type = proc_map_type::section;
        sect.acquire();
    }

    virtual ~mapping_section() {
        sect.release();
    }

    bool map(process& proc, uint32_t offset, uint64_t va);
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;

    section& sect;
};

class mapping_vm : public mapping {
public:
    mapping_vm() {
        type = proc_map_type::vm;
    }

    bool map(process& proc, uint32_t offset, uint64_t va);
    NTSTATUS commit(process& proc, uint32_t offset, uint32_t commit_size,
                    enum mapping_type prot);
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;
};

static const size_t MAX_PATH = 260;

// FIXME - 32-bit processes

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

static mapping* find_mapping(process& proc, uint64_t address) {
    lock_guard lock(proc.mappings_lock);

    address &= ~(PAGE_SIZE - 1);

    for (auto le = proc.mappings.next; le != &proc.mappings; le = le->next) {
        auto& m = containing_record<mapping>(le);

        if (m.address <= address && m.address + m.size > address) {
            m.acquire();
            return &m;
        } else if (m.address > address)
            break;
    }

    return nullptr;
}

static bool allocate_proc_params(process& proc, HANDLE stdinh, HANDLE stdouth, HANDLE stderrh, HANDLE curdirh,
                                 string_view curdir, string_view fn) {
    // FIXME - also include space for command line, environment, etc.

    static_assert(sizeof(RTL_USER_PROCESS_PARAMETERS) <= PAGE_SIZE);

    auto curdir_len = dos_path_len(curdir);
    auto fn_len = dos_path_len(fn);

    size_t size;

    size = sizeof(RTL_USER_PROCESS_PARAMETERS);
    size += MAX_PATH * sizeof(char16_t);
    size += (fn_len + 1) * sizeof(char16_t);

    if (auto status = proc.allocate_vm((void**)&proc.proc_params, 0, &size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write); !success(status)) {
        print("process::allocate_vm returned {}\n", status);
        return false;
    }

    auto num_pages = bytes_to_pages(size);

    auto pm = find_mapping(proc, (uintptr_t)proc.proc_params);
    if (!pm) {
        print("allocate_proc_params: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;
    uint64_t first_page = ((uintptr_t)proc.proc_params - m.address) >> PAGE_SHIFT;

    if (!m.cache.pin(first_page, num_pages, nullptr)) {
        print("allocate_proc_params: page_cache::pin failed\n");
        m.release();
        return false;
    }

    // FIXME - setup cache for writing

    page_cache_entry* p;

    {
        lock_guard lg(m.cache.cache_lock);
        p = m.cache.find_page_cache_entry(first_page);

        if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            if (p->pfn)
                p->status = page_cache_status::clean; // FIXME - or dirty?
        }
    }

    if (p->pfn == 0) {
        print("allocate_folio failed\n");
        m.cache.unpin(first_page, num_pages);
        m.release();
        return false;
    }

    auto& ks_proc_params = *(RTL_USER_PROCESS_PARAMETERS*)get_phys_address(*p);

    ks_proc_params.StandardInput = stdinh;
    ks_proc_params.StandardOutput = stdouth;
    ks_proc_params.StandardError = stderrh;
    ks_proc_params.CurrentDirectory.Handle = curdirh;

    ks_proc_params.CurrentDirectory.DosPath.Length = (uint16_t)(min(MAX_PATH, curdir_len) * sizeof(char16_t));
    ks_proc_params.CurrentDirectory.DosPath.MaximumLength = MAX_PATH * sizeof(char16_t);
    ks_proc_params.CurrentDirectory.DosPath.Buffer = (char16_t*)((uint8_t*)proc.proc_params + sizeof(RTL_USER_PROCESS_PARAMETERS));

    ks_proc_params.ImagePathName.Length = (uint16_t)(fn_len * sizeof(char16_t));
    ks_proc_params.ImagePathName.MaximumLength = ks_proc_params.ImagePathName.Length + sizeof(char16_t);
    ks_proc_params.ImagePathName.Buffer = ks_proc_params.CurrentDirectory.DosPath.Buffer +
        (ks_proc_params.CurrentDirectory.DosPath.MaximumLength / sizeof(char16_t));

    auto dos_path = (char16_t*)malloc(ks_proc_params.CurrentDirectory.DosPath.MaximumLength);
    if (!dos_path) {
        print("malloc failed\n");
        m.cache.unpin(first_page, num_pages);
        m.release();
        return false;
    }

    create_dos_path(span(dos_path, ks_proc_params.CurrentDirectory.DosPath.MaximumLength / sizeof(char16_t)), curdir);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params.CurrentDirectory.DosPath.Buffer - (uintptr_t)proc.proc_params),
                   dos_path, ks_proc_params.CurrentDirectory.DosPath.Length)) {
        print("mapping::copy_to failed\n");
        m.cache.unpin(first_page, num_pages);
        m.release();
        free(dos_path);
        return false;
    }

    free(dos_path);

    auto fn_path = (char16_t*)malloc(ks_proc_params.ImagePathName.MaximumLength);
    if (!fn_path) {
        print("malloc failed\n");
        m.cache.unpin(first_page, num_pages);
        m.release();
        return false;
    }

    create_dos_path(span(fn_path, ks_proc_params.ImagePathName.MaximumLength / sizeof(char16_t)), fn);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params.ImagePathName.Buffer - (uintptr_t)proc.proc_params),
                   fn_path, ks_proc_params.ImagePathName.Length)) {
        print("mapping::copy_to failed\n");
        m.cache.unpin(first_page, num_pages);
        m.release();
        free(fn_path);
        return false;
    }

    free(fn_path);

    m.cache.unpin(first_page, num_pages);
    m.release();

    return true;
}

static bool allocate_peb(process& proc, HANDLE stdinh, HANDLE stdouth,
                         HANDLE stderrh, HANDLE curdirh, string_view curdir,
                         string_view fn) {
    static const auto num_pages = bytes_to_pages(sizeof(PEB));

    size_t peb_size = sizeof(PEB);

    if (!allocate_proc_params(proc, stdinh, stdouth, stderrh, curdirh, curdir, fn)) {
        print("allocate_proc_params failed\n");
        return false;
    }

    if (!success(proc.allocate_vm((void**)&proc.peb, 0, &peb_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        print("process::allocate_vm failed\n");
        return false;
    }

    auto pm = find_mapping(proc, (uintptr_t)proc.peb);
    if (!pm) {
        print("allocate_peb: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;
    uint64_t first_page = ((uintptr_t)proc.peb - m.address) >> PAGE_SHIFT;

    if (!m.cache.pin(first_page, num_pages, nullptr)) {
        print("allocate_peb: page_cache::pin failed\n");
        m.release();
        return false;
    }

    // FIXME - setup cache for writing

    page_cache_entry* p;

    {
        lock_guard lg(m.cache.cache_lock);
        p = m.cache.find_page_cache_entry(first_page);

        if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            if (p->pfn)
                p->status = page_cache_status::clean; // FIXME - or dirty?
        }
    }

    if (p->pfn == 0) {
        print("allocate_folio failed\n");
        m.cache.unpin(first_page, num_pages);
        m.release();
        return false;
    }

    auto& ks_peb = *(PEB*)get_phys_address(*p);

    ks_peb.ImageBaseAddress = (void*)proc.img_addr;
    ks_peb.ProcessParameters = proc.proc_params;

    m.cache.unpin(first_page, num_pages);
    m.release();

    return true;
}

object_ref create_process(string_view fn, string_view curdir,
                          object_ref& stdinobj, object_ref& stdoutobj,
                          object_ref& stderrobj) {
    nt_status status;
    object_ref img_inode_ref, curdir_ref;
    unsigned int create_result;

    // FIXME - merge with NtCreateProcess?

    status = open_inode(fn, nullopt, img_inode_ref, FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE, create_result);
    if (!success(status)) { // FIXME - return status
        print("open_inode failed for {}: {}\n", fn, status);
        return {};
    }

    if (img_inode_ref.get().type != object_type::inode) {
        print("open_inode returned something other than inode\n");
        return {};
    }

    auto& img_inode = static_cast<inode&>(img_inode_ref.get());

    status = open_inode(curdir, nullopt, curdir_ref, FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE, create_result);
    if (!success(status)) { // FIXME - return status
        print("open_inode failed for {}: {}\n", curdir, status);
        return {};
    }

    auto& curdir_obj = curdir_ref.get();
    if (curdir_obj.type != object_type::inode) {
        print("open_inode returned something other than inode\n");
        return {};
    }

    auto& curdir_inode = static_cast<inode&>(curdir_obj);

    auto pptr = new process;
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    // create section for image

    object_ref f(*new file(img_inode));
    object_ref imgh;

    status = create_section(static_cast<file&>(f.get()), true, 0, imgh);
    if (!success(status)) {
        print("create_section returned {}\n", status);
        p.release();
        return {};
    }

    p.img = static_cast<section&>(imgh.get());

    if (p.img->get().machine != IMAGE_FILE_MACHINE_AMD64) {
        print("invalid architecture {:x}\n", p.img->get().machine);
        p.release();
        return {};
    }

    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return {};
    }

    p.img->get().acquire();

    imgh.reset();

    auto& img = p.img->get();

    p.img_addr = p.map_section(img, img.preferred_base + img.size > MAX_USERSPACE_ADDRESS ? 0 : img.preferred_base);
    if (!p.img_addr) {
        print("process::map_section failed\n");
        p.release();
        return {};
    }

    // create stack

    size_t stack_size = max(img.stack_size, (uint64_t)PAGE_SIZE);
    void* stackaddr = nullptr;

    if (auto status = p.allocate_vm(&stackaddr, 0, &stack_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write); !success(status)) {
        print("process::map_section returned {}\n", status);
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

    if (stdinobj) {
        status = p.add_handle(stdinobj.get(), 0, true, stdinh); // FIXME - access mask
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    if (stdoutobj) {
        status = p.add_handle(stdoutobj.get(), 0, true, stdouth); // FIXME - access mask
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    if (stderrobj) {
        status = p.add_handle(stderrobj.get(), 0, true, stderrh); // FIXME - access mask
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    auto curdir_file = new file(curdir_inode);
    if (!curdir_file) {
        print("new failed\n");
        p.release();
        return {};
    }

    handle_t curdirh;

    status = p.add_handle(*curdir_file, 0, true, curdirh); // FIXME - access mask
    if (!success(status)) {
        print("add_handle failed\n");
        curdir_file->release();
        p.release();
        return {}; // FIXME - return status
    }

    curdir_file->release();

    if (!allocate_peb(p, stdinh, stdouth, stderrh, curdirh, curdir, fn)) {
        print("allocate_peb failed\n");
        p.release();
        return {};
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    CONTEXT entry_point_context;
    CLIENT_ID client_id;
    INITIAL_TEB initial_teb;

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stackaddr + stack_size;
    initial_teb.StackLimit = (uint8_t*)stackaddr + stack_size;
    initial_teb.StackAllocationBase = stackaddr;

    uintptr_t entry_point_ctx_addr = (uintptr_t)stackaddr + stack_size - sizeof(CONTEXT) - 0x20;

    memset(&entry_point_context, 0, sizeof(entry_point_context));
    entry_point_context.Rip = p.img_addr + img.entry_point;
    entry_point_context.Rsp = (uintptr_t)stackaddr + stack_size - 0x20; // 0x20 for MS ABI

    if (!p.copy_to((void*)entry_point_ctx_addr, &entry_point_context, sizeof(entry_point_context))) {
        print("process::copy_to failed\n");
        p.release();
        return {};
    }

    status = p.create_thread(nullopt, 0, nullptr, client_id, entry_point_context, initial_teb, true);
    if (!success(status)) {
        print("process::create_thread returned {}\n", status);
        p.release();
        return {};
    }

    p.suspend_count = 1;

    return p;
}

process::~process() {
    free_mappings();

    if (img)
        img->get().release();

    if (cr3)
        free_folio(cr3);
}

static bool set_page_protection(process& proc, uint64_t va,
                                uint32_t num_pages, enum mapping_type protect) {
    span<HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    auto redo_level = [&](uint8_t level) -> bool {
        if (!pt[level][index[level]].p) {
            uint64_t pa = allocate_page();
            if (pa == 0)
                return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            pt[level][index[level]].pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            pt[level][index[level]].p = 1;
            pt[level][index[level]].w = 1;
            pt[level][index[level]].u = 1;

            memset(phys_address(pa), 0, PAGE_SIZE);
        }

        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);

        return true;
    };

    for (uint8_t level = 3; level > 0; level--) {
        if (!redo_level(level))
            return false;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (i != 0) {
            index[0]++;

            if (index[0] == 512) {
                index[0] = 0;
                index[1]++;

                if (index[1] == 512) {
                    index[1] = 0;
                    index[2]++;

                    if (index[2] == 512) {
                        index[2] = 0;
                        index[3]++;

                        redo_level(3);
                    }

                    redo_level(2);
                }

                redo_level(1);
            }
        }

        auto& pte = pt[0][index[0]];

        pte.p = 0;
        pte.u = 1;
        pte.w = 0;

        switch (protect) {
            case mapping_type::read_write:
                pte.xd = 1;
                pte.allow_write = 1;
                break;

            case mapping_type::read_execute:
                pte.xd = 0;
                pte.allow_write = 0;
                break;

            case mapping_type::read_write_execute:
                pte.xd = 0;
                pte.allow_write = 1;
                break;

            default:
                pte.xd = 1;
                pte.allow_write = 0;
                break;
        }
    }

    return true;
}

uintptr_t process::map_section(section& sect, uintptr_t address) {
    // FIXME - offset and mapping size

    if (address & 0x8000000000000000 || ((address + sect.size) & 0x8000000000000000)) { // don't allow in kernel space
        print("invalid address {:x}\n", address);
        return 0;
    }

    if (address & (PAGE_SIZE - 1)) { // must be on page boundary
        print("invalid address {:x}\n", address);
        return 0;
    }

    if (address & 0xffff000000000000 || (address + sect.size) & 0xffff000000000000) { // VAs limited to 48 bits
        print("invalid address {:x}\n", address);
        return 0;
    }

    auto m = new mapping_section(sect);
    if (!m) {
        print("new failed\n");
        return 0;
    }

    m->address = address;
    m->size = sect.size;

    uintptr_t last_addr = 1 << PAGE_SHIFT; // don't allocate VM address 0
    bool inserted = false;

    {
        lock_guard lock(mappings_lock);

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m2 = containing_record<mapping>(le);

            if (address == 0) {
                if (m2.address > last_addr + m->size) { // insert before
                    address = last_addr;
                    m->address = address;
                    insert_head_list(*le->prev, m->le);
                    inserted = true;
                    break;
                }

                last_addr = m2.address + m2.size;
            } else {
                if (m2.address > address + m->size) { // insert before
                    m->address = address;
                    insert_head_list(*le->prev, m->le);
                    inserted = true;
                    break;
                }

                if (address < m2.address + m2.size && address + m->size > m2.address) { // collision
                    print("collision\n");
                    delete m;
                    return 0;
                }
            }

            le = le->next;
        }

        if (address == 0) {
            if (last_addr >= MAX_USERSPACE_ADDRESS - m->size)
                return 0;

            address = last_addr;
        }

        m->address = address;

        if (!inserted)
            insert_tail_list(mappings, m->le);
    }

    if (sect.pe_sections) {
        uint64_t addr = 0;

        for (unsigned int i = 0; i < sect.num_pe_sections; i++) {
            auto& s = sect.pe_sections[i];

            if (s.VirtualAddress > addr) {
                if (!set_page_protection(*this, address + addr, (uint32_t)((s.VirtualAddress - addr) >> PAGE_SHIFT),
                                         mapping_type::read_only)) {
                    return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
                }
            }

            uint64_t num_pages = bytes_to_pages(s.VirtualSize);

            if (!set_page_protection(*this, address + s.VirtualAddress, (uint32_t)num_pages,
                                     characteristics_to_mapping_type(s.Characteristics))) {
                return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
            }

            addr = s.VirtualAddress + (num_pages << PAGE_SHIFT);
        }

        if (addr < sect.size) {
            if (!set_page_protection(*this, address + addr, (uint32_t)((sect.size - addr) >> PAGE_SHIFT),
                                     mapping_type::read_only)) {
                return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
            }
        }
    } else {
        // FIXME - use section's specified protection
    }

    return address;
}

static bool populate_teb(process& proc, thread& t) {
    static const auto num_pages = 1; // only care about first page

    static_assert(offsetof(TEB, NtTib.Self) >> PAGE_SHIFT == 0);
    static_assert(offsetof(TEB, ProcessEnvironmentBlock) >> PAGE_SHIFT == 0);

    // we're assuming that TEB lives at the beginning of a page
    assert(!((uintptr_t)t.teb & (PAGE_SIZE - 1)));

    auto pm = find_mapping(proc, (uintptr_t)t.teb);
    if (!pm) {
        print("populate_teb: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;
    uint64_t first_page = ((uintptr_t)t.teb - m.address) >> PAGE_SHIFT;

    if (!m.cache.pin(first_page, num_pages, nullptr)) {
        print("populate_teb: page_cache::pin failed\n");
        m.release();
        return false;
    }

    // FIXME - setup cache for writing

    page_cache_entry* p;

    {
        lock_guard lg(m.cache.cache_lock);
        p = m.cache.find_page_cache_entry(first_page);

        if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            if (p->pfn)
                p->status = page_cache_status::clean; // FIXME - or dirty?
        }
    }

    if (p->pfn == 0) {
        print("allocate_folio failed\n");
        m.cache.unpin(first_page, num_pages);
        m.release();
        return false;
    }

    auto& teb = *(TEB*)get_phys_address(*p);

    teb.NtTib.Self = &t.teb->NtTib;
    // FIXME - StackBase
    // FIXME - StackLimit
    // FIXME - ClientId
    teb.ProcessEnvironmentBlock = proc.peb;

    m.cache.unpin(first_page, num_pages);
    m.release();

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

static HARDWARE_PTE* find_pte(process& proc, uintptr_t va) {
    span<HARDWARE_PTE> pt[4];

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    if (!pt[3][(va & 0xff8000000000) >> 39].p)
        return nullptr;

    pt[2] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[3][(va & 0xff8000000000) >> 39].pfn) << PAGE_SHIFT)), 512);

    if (!pt[2][(va & 0x7fc0000000) >> 30].p)
        return nullptr;

    pt[1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[2][(va & 0x7fc0000000) >> 30].pfn) << PAGE_SHIFT)), 512);

    if (!pt[1][(va & 0x3fe00000) >> 21].p)
        return nullptr;

    pt[0] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[1][(va & 0x3fe00000) >> 21].pfn) << PAGE_SHIFT)), 512);

    return &pt[0][(va & 0x1ff000) >> 12];
}

bool process::handle_page_fault(uintptr_t address, uint64_t error_code) {
    page_fault_error_code ec;

    ec.value = error_code;

    if (ec.present) {
        if (ec.instruction_fetch) // NX violation
            return false;

        auto pte = find_pte(*this, address);
        if (!pte)
            return false;

        if (pte->allow_write && !pte->w) {
            // FIXME - mark cache as dirty
            pte->w = 1;
            return true;
        }

        return false;
    }

    address &= ~(PAGE_SIZE - 1);

    auto m = find_mapping(*this, address);
    if (!m)
        return false;

    bool ret;

    switch (m->type) {
        case proc_map_type::section: {
            auto& ms = static_cast<mapping_section&>(*m);

            // FIXME - return false if actual page violation

            ret = ms.map(*this, (uint32_t)(address - m->address), address);
            break;
        }

        case proc_map_type::vm:
            ret = static_cast<mapping_vm&>(*m).map(*this, (uint32_t)(address - m->address), address);
            break;

        default:
            ret = false;
            break;
    }

    m->release();
    return ret;
}

bool process::allocate_cr3() {
    cr3 = allocate_page();
    if (!cr3) {
        print("allocate_page failed\n");
        return false;
    }

    auto va = phys_address(cr3);

    // copy higher half mappings

    memset((void*)va, 0, PAGE_SIZE >> 1);
    memcpy((uint8_t*)va + (PAGE_SIZE >> 1), (uint8_t*)SELFMAP_PML4 + (PAGE_SIZE >> 1), PAGE_SIZE >> 1);

    auto pml4t = (HARDWARE_PTE*)va;

    // add recursive mapping
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pml4t[(SELFMAP & 0xff8000000000) >> 39].pfn = cr3 >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pml4t[(SELFMAP & 0xff8000000000) >> 39].p = 1;
    pml4t[(SELFMAP & 0xff8000000000) >> 39].w = 1;

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

                if (!m.copy_from((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
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

                if (!m.copy_to((uint32_t)((uintptr_t)us_addr - m.address), ks_addr, (uint32_t)to_copy))
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
    nt_status status;
    object_ref inode_ref;
    unsigned int create_result;

    status = open_inode("/ntdll.dll", nullopt, inode_ref, FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE, create_result);
    if (!success(status))
        bsod_format("open_inode returned {} for ntdll.dll", status);

    if (inode_ref.get().type != object_type::inode)
        bsod("open_inode returned something other than an inode");

    object_ref f(*new file(inode_ref.get()));
    object_ref ntdllh;

    status = create_section(static_cast<file&>(f.get()), true, 0, ntdllh);
    if (!success(status))
        bsod_format("create_section returned {} for ntdll.dll", status);

    ntdll = &static_cast<section&>(ntdllh.get());
    ntdll->acquire();

    mn_start_thread = ntdll->find_export("mn_start_thread");
    if (!mn_start_thread)
        bsod("Could not find export mn_start_thread in ntdll.dll");

    mn_signal = ntdll->find_export("mn_signal");
    if (!mn_signal)
        bsod("Could not find export mn_signal in ntdll.dll");
}

void process::terminate() {
    // wake up threads waiting for process to terminate

    {
        sync_lock sl;
        lock_guard lock(sl);

        wake_up_waiters(waiters);
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
            auto pa = va_to_pa_lookup((uintptr_t)handle_arr + (i << PAGE_SHIFT));

            unmap_page((uintptr_t)handle_arr + (i << PAGE_SHIFT));
            free_folio(pa);
        }

        free_va((uintptr_t)handle_arr, handle_arr_pages);
    }

    free_mappings();
}

void process::free_mappings() {
    while (!is_list_empty(mappings)) {
        auto& m = containing_record<mapping>(mappings.next);

        remove_entry_list(m.le);

        m.release();
    }
}

NTSTATUS NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status) {
    object_ref objrw;
    process* p;
    bool this_process;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};

        this_process = true;
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);

        this_process = p == get_current_thread()->proc;
    }

    auto t = get_current_thread();

    while (true) {
        thread* t2;

        {
            lock_guard lock(p->threads_lock);

            if (is_list_empty(p->threads))
                break;

            t2 = (thread*)((uint8_t*)p->threads.next - offsetof(thread, process_le));

            if (t2 == t) { // skip current thread
                if (t2->process_le.next == &p->threads)
                    break;

                t2 = (thread*)((uint8_t*)t2->process_le.next - offsetof(thread, process_le));
            }

            t2->acquire();
        }

        t2->kill(exit_status);
        t2->release();
    }

    if (!this_process)
        return nt_status::SUCCESS;

    t->exit_status = exit_status;

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
    object_ref objrw;
    process* p;

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    p->resume();

    return nt_status::SUCCESS;
}

nt_status process::add_handle(object& obj, uint32_t access_mask,
                              bool allow_shared, handle_t& h) {
    // FIXME - reusing handles?

    if (obj.handle_count.fetch_add(1) > 0 && !allow_shared) {
        obj.handle_count--;
        return nt_status::SHARING_VIOLATION;
    }

    {
        lock_guard lock(handle_lock);

        auto needed_pages = bytes_to_pages((no_handles + 1) * sizeof(handle_obj));

        if (needed_pages > handle_arr_pages) {
            auto pa = allocate_page();
            if (!pa) {
                // FIXME - should be exception
                print("allocate_page failed\n");
                obj.handle_count--;
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            auto va = allocate_va(needed_pages);
            if (!va) {
                print("allocate_page failed\n");
                free_folio(pa);
                obj.handle_count--;
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            if (handle_arr) {
                // remap pages

                for (unsigned int i = 0; i < handle_arr_pages; i++) {
                    auto pa = va_to_pa_lookup((uintptr_t)handle_arr + (i << PAGE_SHIFT));

                    if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
                        print("map_page failed\n");

                        for (unsigned int j = 0; j < i; j++) {
                            unmap_page(va + (j << PAGE_SHIFT));
                        }

                        free_va(va, needed_pages);

                        obj.handle_count--;

                        return nt_status::INSUFFICIENT_RESOURCES;
                    }
                }
            }

            if (!map_page(pa, va + (handle_arr_pages << PAGE_SHIFT), mapping_type::read_write)) {
                // FIXME - exception
                print("map_page failed\n");
                free_folio(pa);

                for (unsigned int i = 0; i < handle_arr_pages; i++) {
                    unmap_page(va + (i << PAGE_SHIFT));
                }

                free_va(va, needed_pages);

                obj.handle_count--;

                return nt_status::INSUFFICIENT_RESOURCES;
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

    return nt_status::SUCCESS;
}

NTSTATUS process::allocate_vm(void** base_address, uintptr_t, size_t* region_size,
                              uint32_t allocation_type, enum mapping_type protect) {
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

        while (true) {
            auto m = find_mapping(*this, addr);

            if (!m)
                return nt_status::NO_MEMORY;

            switch (m->type) {
                case proc_map_type::vm: {
                    auto& mv = static_cast<mapping_vm&>(*m);

                    auto to_commit = min(size, m->address + m->size - addr);

                    auto ret = mv.commit(*this, (uint32_t)(addr - m->address),
                                         (uint32_t)to_commit, protect);
                    if (!success(ret)) {
                        m->release();
                        return ret;
                    }

                    if (to_commit == size) {
                        m->release();
                        return nt_status::SUCCESS;
                    }

                    addr += to_commit;
                    size -= to_commit;

                    break;
                }

                case proc_map_type::section:
                    m->release();
                    return nt_status::NO_MEMORY;
            }

            m->release();
        }

        return nt_status::NO_MEMORY;
    }

    auto m = new mapping_vm;
    if (!m) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    m->size = (uint32_t)size;

    if (!(allocation_type & MEM_COMMIT))
        protect = mapping_type::reserved;

    uintptr_t last_addr = 0x100000; // don't allocate in first megabyte
    bool inserted = false;

    {
        lock_guard lock(mappings_lock);

        for (auto le = mappings.next; le != &mappings; le = le->next) {
            auto& m2 = containing_record<mapping>(le);

            if (addr == 0) {
                if (m2.address > last_addr + m->size) { // insert before
                    insert_head_list(*le->prev, m->le);
                    addr = last_addr;
                    inserted = true;
                    break;
                }

                last_addr = m2.address + m2.size;
            } else {
                if (m2.address > addr + m->size) { // insert before
                    insert_head_list(*le->prev, m->le);
                    inserted = true;
                    break;
                }

                if (addr < m2.address + m2.size && addr + m->size > m2.address) { // collision
                    print("collision\n");
                    m->release();
                    return nt_status::NO_MEMORY;
                }
            }
        }

        if (addr == 0) {
            if (last_addr >= MAX_USERSPACE_ADDRESS - m->size) {
                m->release();
                return nt_status::NO_MEMORY;
            }

            addr = last_addr;
        }

        if (!inserted)
            insert_tail_list(mappings, m->le);

        m->address = addr;

        if (protect != mapping_type::reserved) {
            if (!set_page_protection(*this, addr, num_pages, protect)) {
                print("set_page_protection failed\n");
                remove_entry_list(m->le);
                m->release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }
        }
    }

    *base_address = (void*)addr;
    *region_size = size;

    return nt_status::SUCCESS;
}

NTSTATUS NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                 size_t* region_size, uint32_t allocation_type, uint32_t protect) {
    object_ref objrw;
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

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->allocate_vm(base_address, zero_bits, region_size, allocation_type, prot);
}

void process::unmap_proc_page(uint64_t va) {
    span<HARDWARE_PTE> pml4, pdpt, pdt, pt;

    pml4 = span((HARDWARE_PTE*)phys_address(cr3), 512);

    if (!pml4[(va & 0xff8000000000) >> 39].p)
        return;

    pdpt = span((HARDWARE_PTE*)phys_address((uint64_t)((pml4[(va & 0xff8000000000) >> 39].pfn) << PAGE_SHIFT)), 512);

    if (!pdpt[(va & 0x7fc0000000) >> 30].p)
        return;

    pdt = span((HARDWARE_PTE*)phys_address((uint64_t)((pdpt[(va & 0x7fc0000000) >> 30].pfn) << PAGE_SHIFT)), 512);

    if (!pdt[(va & 0x3fe00000) >> 21].p)
        return;

    pt = span((HARDWARE_PTE*)phys_address((uint64_t)((pdt[(va & 0x3fe00000) >> 21].pfn) << PAGE_SHIFT)), 512);

    pt[(va & 0x1ff000) >> 12].p = 0;
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

        for (auto le = mappings.next; le != &mappings; le = le->next) {
            auto& m2 = containing_record<mapping>(le);

            if (m2.address > (uintptr_t)*base_address)
                break;
            else if (m2.address < (uintptr_t)*base_address)
                continue;

            // FIXME - MEM_RELEASE with reserved memory block
            if (m2.type != proc_map_type::vm)
                return nt_status::INVALID_PARAMETER;

            auto& mv = static_cast<mapping_vm&>(m2);

            auto num_pages = mv.size >> PAGE_SHIFT;

            // FIXME - also unmap for threads running on other CPUs

            for (unsigned int i = 0; i < num_pages; i++) {
                unmap_proc_page(m2.address + (i << PAGE_SHIFT));
            }

            remove_entry_list(m2.le);
            pm = &m2;
            break;
        }
    }

    if (!pm)
        return nt_status::INVALID_PARAMETER;

    auto& m = *pm;

    // FIXME - if current process running on other cores and mapping changed, send IPI to clear TLB

    *base_address = (void*)m.address;
    *region_size = m.size;

    m.release();

    return nt_status::SUCCESS;
}

NTSTATUS NtFreeVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                             uint32_t free_type) {
    object_ref objrw;
    process* p;

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->free_vm(base_address, region_size, free_type);
}

static void process_map_page(process& proc, uint64_t pfn, uint64_t va) {
    span<HARDWARE_PTE> pt[4];

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    assert(pt[3][(va & 0xff8000000000) >> 39].p);
    pt[2] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[3][(va & 0xff8000000000) >> 39].pfn) << PAGE_SHIFT)), 512);

    assert(pt[2][(va & 0x7fc0000000) >> 30].p);
    pt[1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[2][(va & 0x7fc0000000) >> 30].pfn) << PAGE_SHIFT)), 512);

    assert(pt[1][(va & 0x3fe00000) >> 21].p);
    pt[0] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[1][(va & 0x3fe00000) >> 21].pfn) << PAGE_SHIFT)), 512);

    auto& pte = pt[0][(va & 0x1ff000) >> 12];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pte.pfn = pfn;
#pragma GCC diagnostic pop
    pte.p = 1;
}

bool mapping_vm::map(process& proc, uint32_t offset, uint64_t va) {
    uint64_t page_num = offset >> PAGE_SHIFT;

    if (cache.read_into_cache_and_pin(page_num, 1) == 0)
        return false;

    page_cache_entry* p;

    {
        lock_guard lg(cache.cache_lock);
        p = cache.find_page_cache_entry(page_num);
    }

    process_map_page(proc, p->pfn, va);

    cache.unpin(page_num, 1); // FIXME?

    return true;
}

bool mapping_section::map(process& proc, uint32_t offset, uint64_t va) {
    uint64_t page_num = offset >> PAGE_SHIFT;

    if (cache.read_into_cache_and_pin(page_num, 1) == 0)
        return false;

    page_cache_entry* p;

    {
        lock_guard lg(cache.cache_lock);
        p = cache.find_page_cache_entry(page_num);
    }

    process_map_page(proc, p->pfn, va);

    cache.unpin(page_num, 1); // FIXME?

    return true;
}

bool mapping::copy_from(uint32_t offset, void* ks_addr, uint32_t size) {
    uint64_t page_num = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + size + PAGE_SIZE - 1) >> PAGE_SHIFT) - page_num;

    auto pinned_pages = cache.read_into_cache_and_pin(page_num, num_pages);

    if (pinned_pages < num_pages) {
        print("mapping::copy_from: read_into_cache_and_pin returned {}, expected {}\n",
              pinned_pages, num_pages);
        cache.unpin(page_num, pinned_pages);
        return false;
    }

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(page_num);
            else
                p = cache.next_page_cache_entry(*p);
        }

        uint32_t to_copy, page_off;

        if (i == 0) {
            page_off = offset & (PAGE_SIZE - 1);
            to_copy = min(size, (uint32_t)(PAGE_SIZE - page_off));
        } else if (i == num_pages - 1) {
            page_off = 0;
            to_copy = (offset + size) & (PAGE_SIZE - 1);

            if (to_copy == 0)
                to_copy = PAGE_SIZE;
        } else {
            page_off = 0;
            to_copy = PAGE_SIZE;
        }

        memcpy(ks_addr, (uint8_t*)get_phys_address(*p) + page_off, to_copy);

        ks_addr = (uint8_t*)ks_addr + to_copy;
    }

    cache.unpin(page_num, num_pages);

    return true;
}

bool mapping::copy_to(uint32_t offset, const void* ks_addr, uint32_t size) {
    uint64_t page_num = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + size + PAGE_SIZE - 1) >> PAGE_SHIFT) - page_num;

    if (!cache.pin(page_num, num_pages, nullptr)) {
        print("mapping::copy_to: page_cache::pin failed\n");
        return false;
    }

    // read first page if needed
    if (offset & (PAGE_SIZE - 1)) {
        // FIXME - easy way to do this without pinning again?
        if (!cache.read_into_cache_and_pin(page_num, 1)) {
            print("mapping::copy_to: page_cache::read_into_cache_and_pin failed\n");
            cache.unpin(page_num, num_pages);
            return false;
        }

        cache.unpin(page_num, 1);
    }

    // read last page if needed
    if ((offset + size) & (PAGE_SIZE - 1) && (num_pages != 1 || !(offset & (PAGE_SIZE - 1)))) {
        // FIXME - easy way to do this without pinning again?
        if (!cache.read_into_cache_and_pin(page_num + num_pages - 1, 1)) {
            print("mapping::copy_to: page_cache::read_into_cache_and_pin failed\n");
            cache.unpin(page_num, num_pages);
            return false;
        }

        cache.unpin(page_num + num_pages - 1, 1);
    }

    // FIXME - COWing

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(page_num);
            else
                p = cache.next_page_cache_entry(*p);

            if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (p->pfn)
                    p->status = page_cache_status::clean; // FIXME - or dirty?
            }
        }

        if (!p->pfn) {
            print("allocate_folio failed\n");
            cache.unpin(page_num, num_pages);
            return false;
        }

        uint32_t to_copy, page_off;

        if (i == 0) {
            page_off = offset & (PAGE_SIZE - 1);
            to_copy = min(size, (uint32_t)(PAGE_SIZE - page_off));
        } else if (i == num_pages - 1) {
            page_off = 0;
            to_copy = (offset + size) & (PAGE_SIZE - 1);

            if (to_copy == 0)
                to_copy = PAGE_SIZE;
        } else {
            page_off = 0;
            to_copy = PAGE_SIZE;
        }

        memcpy((uint8_t*)get_phys_address(*p) + page_off, ks_addr, to_copy);

        ks_addr = (uint8_t*)ks_addr + to_copy;
    }

    cache.unpin(page_num, num_pages);

    return true;
}

object_ref process::deref_handle(handle_t h) {
    if (h == NtCurrentProcess()) {
        this->acquire();
        return *this;
    }

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

static enum mapping_type get_page_protection(process& proc, uint64_t va) {
    span<HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    for (uint8_t level = 3; level > 0; level--) {
        if (!pt[level][index[level]].p)
            return mapping_type::no_access;

        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    }

    auto& pte = pt[0][index[0]];

    if (pte.xd) {
        if (pte.w)
            return mapping_type::read_write;
        else
            return mapping_type::read_only;
    } else {
        if (pte.w)
            return mapping_type::read_write_execute;
        else
            return mapping_type::read_execute;
    }
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

    bool first = true;
    lock_guard lock(mappings_lock);

    auto le = mappings.next;
    while (le != &mappings) {
        auto& m = containing_record<mapping>(le);

        if (m.address > addr + (pages << PAGE_SHIFT))
            return nt_status::NO_MEMORY;

        if (m.address + m.size > addr && m.address < addr + (pages << PAGE_SHIFT)) {
            auto num_pages = min(pages, (uint32_t)((m.address + m.size - addr) >> PAGE_SHIFT));

            if (first)
                old_prot = get_page_protection(*this, addr);

            if (!set_page_protection(*this, addr, num_pages, prot))
                return nt_status::INSUFFICIENT_RESOURCES;

            first = false;

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
    object_ref objrw;
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

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (proc_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(proc_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    enum mapping_type old_type;

    auto ret = p->protect_vm(base_address, region_size, prot, old_type);

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

            default:
                *old_prot = PAGE_NOACCESS;
                break;
        }
    }

    return ret;
}

NTSTATUS mapping_vm::commit(process& proc, uint32_t offset, uint32_t commit_size,
                            enum mapping_type prot) {
    auto num_pages = commit_size >> PAGE_SHIFT;

    if (!set_page_protection(proc, address + offset, num_pages, prot))
        return nt_status::INSUFFICIENT_RESOURCES;

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
    uint64_t num_pages;
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

                num_pages = ms.size >> PAGE_SHIFT;

                for (unsigned int i = 0; i < num_pages; i++) {
                    // FIXME - unmap_proc_page should take range
                    unmap_proc_page(m.address + (i << PAGE_SHIFT));
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

NTSTATUS NtCreateProcess(HANDLE* process_handle, ACCESS_MASK, const OBJECT_ATTRIBUTES*,
                         HANDLE, bool, HANDLE section_handle, HANDLE, HANDLE) {
    object_ref sect;

    // FIXME - desired_access
    // FIXME - object_attributes
    // FIXME - parent_process
    // FIXME - inherit_object_table
    // FIXME - debug_port
    // FIXME - exception_port

    sect = deref_handle(section_handle);
    if (!sect)
        return nt_status::INVALID_HANDLE;

    if (sect.get().type != object_type::section)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& img = *static_cast<section*>(&sect.get());

    if (img.type != section_type::image)
        return nt_status::INVALID_PARAMETER;

    auto pptr = new process;
    if (!pptr)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto& p = *pptr;

    p.img = img;
    img.acquire();

    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    p.img_addr = p.map_section(img, img.preferred_base + img.size > MAX_USERSPACE_ADDRESS ? 0 : img.preferred_base);
    if (!p.img_addr) {
        print("process::map_section failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - if ntdll would have collision, choose another address

    p.ntdll_addr = p.map_section(*ntdll, ntdll->preferred_base);
    if (!p.ntdll_addr) {
        print("process::map_section failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - map time page into process

    if (!allocate_peb(p, 0, 0, 0, 0, "", string_view(img.name, img.namelen))) {
        print("allocate_peb failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    p.suspend_count = 0;

    auto& curproc = *get_current_thread()->proc;

    handle_t h;

    auto status = curproc.add_handle(p, 0, true, h); // FIXME - access mask
    if (!success(status)) {
        p.release();
        return status;
    }

    p.release(); // add_handle increases refcount

    *process_handle = h;

    return nt_status::SUCCESS;
}

NTSTATUS NtDuplicateObject(HANDLE source_process_handle, HANDLE source_handle, HANDLE target_process_handle,
                           HANDLE* target_handle, ACCESS_MASK, uint32_t, uint32_t) {
    object_ref srcprocobj, destprocobj;
    process* srcproc;
    process* destproc;

    // FIXME - desired_access
    // FIXME - handle_attributes
    // FIXME - options (DUPLICATE_SAME_ATTRIBUTES, DUPLICATE_SAME_ACCESS, DUPLICATE_CLOSE_SOURCE)

    if (source_process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        srcproc = t->proc;
        srcproc->acquire();

        srcprocobj = object_ref{*srcproc};
    } else {
        srcprocobj = deref_handle(source_process_handle);
        if (!srcprocobj)
            return nt_status::INVALID_HANDLE;

        auto& obj = srcprocobj.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        srcproc = static_cast<process*>(&obj);
    }

    if (target_process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        destproc = t->proc;
        destproc->acquire();

        destprocobj = object_ref{*destproc};
    } else {
        destprocobj = deref_handle(target_process_handle);
        if (!destprocobj)
            return nt_status::INVALID_HANDLE;

        auto& obj = destprocobj.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        destproc = static_cast<process*>(&obj);
    }

    // FIXME - source_handle of NtCurrentProcess() or NtCurrentThread() (what if source_process_handle points to different process?)

    auto objrw = srcproc->deref_handle(source_handle);

    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    handle_t h;
    auto status = destproc->add_handle(obj, 0, true, h); // FIXME - access mask

    if (!success(status))
        return status;

    if (target_handle)
        *target_handle = h;

    return nt_status::SUCCESS;
}

NTSTATUS process::read_vm(void* base_address, void* buffer, uint32_t buffer_size,
                          uint32_t* number_of_bytes_read) {
    uint32_t copied = 0;

    while (true) {
        lock_guard lock(mappings_lock);
        bool found = false;

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (m.address <= (uintptr_t)base_address && m.address + m.size > (uintptr_t)base_address) {
                auto to_copy = min(buffer_size, (uint32_t)(m.address + m.size - (uintptr_t)base_address));

                switch (m.type) {
                    case proc_map_type::section:
                        if (!static_cast<mapping_section&>(m).copy_from((uint32_t)((uintptr_t)base_address - m.address), buffer, (uint32_t)to_copy))
                            return nt_status::INTERNAL_ERROR;
                        break;

                    case proc_map_type::vm:
                        if (!static_cast<mapping_vm&>(m).copy_from((uint32_t)((uintptr_t)base_address - m.address), buffer, (uint32_t)to_copy))
                            return nt_status::INTERNAL_ERROR;
                        break;
                }

                copied += to_copy;

                if (buffer_size == to_copy)
                    goto end;

                base_address = (uint8_t*)base_address + to_copy;
                buffer = (uint8_t*)buffer + to_copy;
                buffer_size -= to_copy;
                found = true;

                break;
            }

            le = le->next;
        }

        if (!found)
            break;
    }

end:
    if (copied == 0)
        return nt_status::NO_MEMORY;

    if (number_of_bytes_read)
        *number_of_bytes_read = copied;

    return nt_status::SUCCESS;
}

NTSTATUS NtReadVirtualMemory(HANDLE process_handle, void* base_address, void* buffer, uint32_t buffer_size,
                             uint32_t* number_of_bytes_read) {
    object_ref objrw;
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->read_vm(base_address, buffer, buffer_size, number_of_bytes_read);
}

NTSTATUS process::write_vm(void* base_address, void* buffer, uint32_t buffer_size,
                           uint32_t* number_of_bytes_written) {
    uint32_t copied = 0;

    while (true) {
        lock_guard lock(mappings_lock);
        bool found = false;

        auto le = mappings.next;
        while (le != &mappings) {
            auto& m = containing_record<mapping>(le);

            if (m.address <= (uintptr_t)base_address && m.address + m.size > (uintptr_t)base_address) {
                auto to_copy = min(buffer_size, (uint32_t)(m.address + m.size - (uintptr_t)base_address));

                switch (m.type) {
                    case proc_map_type::section:
                        if (!static_cast<mapping_section&>(m).copy_to((uint32_t)((uintptr_t)base_address - m.address), buffer, (uint32_t)to_copy))
                            return nt_status::INTERNAL_ERROR;
                        break;

                    case proc_map_type::vm:
                        if (!static_cast<mapping_vm&>(m).copy_to((uint32_t)((uintptr_t)base_address - m.address), buffer, (uint32_t)to_copy))
                            return nt_status::INTERNAL_ERROR;
                        break;
                }

                copied += to_copy;

                if (buffer_size == to_copy)
                    goto end;

                base_address = (uint8_t*)base_address + to_copy;
                buffer = (uint8_t*)buffer + to_copy;
                buffer_size -= to_copy;
                found = true;

                break;
            }

            le = le->next;
        }

        if (!found)
            break;
    }

end:
    if (copied == 0)
        return nt_status::NO_MEMORY;

    if (number_of_bytes_written)
        *number_of_bytes_written = copied;

    return nt_status::SUCCESS;
}

NTSTATUS NtWriteVirtualMemory(HANDLE process_handle, void* base_address, void* buffer, uint32_t buffer_size,
                              uint32_t* number_of_bytes_written) {
    object_ref objrw;
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->write_vm(base_address, buffer, buffer_size, number_of_bytes_written);
}

NTSTATUS process::query_info(enum PROCESSINFOCLASS process_information_class, span<uint8_t> buf,
                             uint32_t* return_length) {
    switch (process_information_class) {
        case PROCESSINFOCLASS::BasicInformation: {
            if (buf.size() < sizeof(PROCESS_BASIC_INFORMATION))
                return nt_status::INFO_LENGTH_MISMATCH;

            auto& pbi = *(PROCESS_BASIC_INFORMATION*)buf.data();

            pbi.ExitStatus = exit_status;
            pbi.PebBaseAddress = peb;
            pbi.AffinityMask = 0xffffffff; // FIXME
            pbi.BasePriority = 0; // FIXME
            pbi.UniqueProcessId = 0; // FIXME - PID
            pbi.InheritedFromUniqueProcessId = 0; // FIXME

            if (return_length)
                *return_length = sizeof(PROCESS_BASIC_INFORMATION);

            return nt_status::SUCCESS;
        }

        default:
            print("process::query_info: unhandled info class {:x}\n",
                  (unsigned int)process_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

NTSTATUS NtQueryInformationProcess(HANDLE process_handle, enum PROCESSINFOCLASS process_information_class,
                                   void* process_information, uint32_t process_information_length,
                                   uint32_t* return_length) {
    object_ref objrw;
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();

        objrw = object_ref{*p};
    } else {
        objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    return p->query_info(process_information_class, span((uint8_t*)process_information, process_information_length),
                         return_length);
}

object_ref process::get_stderr() {
    PEB peb;
    RTL_USER_PROCESS_PARAMETERS params;

    if (!copy_from(&peb, this->peb, sizeof(PEB)))
        return nullopt;

    if (!copy_from(&params, peb.ProcessParameters, sizeof(RTL_USER_PROCESS_PARAMETERS)))
        return nullopt;

    return this->deref_handle(params.StandardError);
}

void process::send_signal(uint64_t code) {
    optional<reference_wrapper<thread>> rw;

    // try to choose an active thread

    {
        lock_guard lg(threads_lock);

        for (auto le = threads.next; le != &threads; le = le->next) {
            auto& t = *(thread*)((uint8_t*)le - offsetof(thread, process_le));

            if (t.affinity == 0)
                continue;

            if (t.suspend_count != 0)
                continue;

            rw = t;
            break;
        }

        if (rw.has_value())
            rw->get().acquire();
    }

    if (!rw.has_value())
        return;

    auto& t = rw->get();

    t.send_signal(code);

    t.release();
}

size_t mapping_section::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    auto pinned_pages = sect.cache.read_into_cache_and_pin(page_num, num_pages);

    if (pinned_pages == 0)
        return 0;

    page_cache_entry* src_p;
    page_cache_entry* dest_p;
    auto read_pages = pinned_pages;

    for (unsigned int i = 0; i < pinned_pages; i++) {
        {
            lock_guard lg(sect.cache.cache_lock);

            if (i == 0)
                src_p = sect.cache.find_page_cache_entry(page_num);
            else
                src_p = sect.cache.next_page_cache_entry(*src_p);
        }

        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);

            if (dest_p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!dest_p->pfn) {
                    print("allocate folio failed\n");
                    read_pages = i;
                    break;
                }
            }
        }

        memcpy(get_phys_address(*dest_p), get_phys_address(*src_p), PAGE_SIZE);
        dest_p->status = page_cache_status::clean;
    }

    sect.cache.unpin(page_num, pinned_pages);

    return read_pages;
}

size_t mapping_section::flush_cache(uint64_t page_num, uint64_t num_pages) {
    print("FIXME - mapping_section::flush_cache({:x}, {:x})\n", page_num, num_pages); // FIXME

    return 0;
}

size_t mapping_vm::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    page_cache_entry* p;

    // FIXME - reflink the zero page

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(page_num);
            else
                p = cache.next_page_cache_entry(*p);

            if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!p->pfn) {
                    print("allocate folio failed\n");
                    return i;
                }
            }
        }

        memset(get_phys_address(*p), 0, PAGE_SIZE);
        p->status = page_cache_status::clean;
    }

    return num_pages;
}

size_t mapping_vm::flush_cache(uint64_t page_num, uint64_t num_pages) {
    print("FIXME - mapping_vm::flush_cache({:x}, {:x})\n", page_num, num_pages); // FIXME

    return 0;
}

NTSTATUS process::create_thread(optional<reference_wrapper<handle_t>> thread_handle, ACCESS_MASK,
                                const OBJECT_ATTRIBUTES*, CLIENT_ID& client_id,
                                const CONTEXT& thread_context,
                                const INITIAL_TEB& initial_teb, bool create_suspended) {
    uint16_t tid;
    CONTEXT start_context;
    mn_image_info ntdll_info, img_info;

    // FIXME - desired_access
    // FIXME - object_attributes

    auto tptr = new (align_val_t(16)) thread;
    if (!tptr)
        return nt_status::INSUFFICIENT_RESOURCES;;

    auto& t = *tptr;

    // FIXME - spinlock for last_tid (or use atomics)
    if (last_tid == 0xffff) { // FIXME - check for lowest unused number
        print("Thread limit reached.\n");
        delete &t;
        return nt_status::INTERNAL_ERROR; // FIXME
    }

    last_tid++;
    tid = last_tid;

    t.tid = tid;

    t.teb = nullptr;
    size_t teb_size = sizeof(TEB);

    if (!success(allocate_vm((void**)&t.teb, 0, &teb_size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write))) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!populate_teb(*this, t)) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(&start_context, &thread_context, sizeof(CONTEXT));
    start_context.EFlags |= 0x202; // IF, reserved flag
    start_context.SegCs = user_CS;
    start_context.SegSs = user_DS;
    start_context.SegEs = user_DS;
    start_context.SegFs = user_DS;
    start_context.SegGs = user_DS;

    auto ctx_addr = (uintptr_t)initial_teb.StackBase - sizeof(CONTEXT);

    if (!copy_to((void*)ctx_addr, &start_context, sizeof(start_context))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    ntdll_info.base = (void*)ntdll_addr;
    ntdll_info.offset_debug_info = ntdll->dwinfo.debug_info.offset;
    ntdll_info.size_debug_info = ntdll->dwinfo.debug_info.size;
    ntdll_info.offset_debug_abbrev = ntdll->dwinfo.debug_abbrev.offset;
    ntdll_info.size_debug_abbrev = ntdll->dwinfo.debug_abbrev.size;
    ntdll_info.offset_debug_rnglists = ntdll->dwinfo.debug_rnglists.offset;
    ntdll_info.size_debug_rnglists = ntdll->dwinfo.debug_rnglists.size;
    ntdll_info.offset_debug_str = ntdll->dwinfo.debug_str.offset;
    ntdll_info.size_debug_str = ntdll->dwinfo.debug_str.size;
    ntdll_info.offset_debug_addr = ntdll->dwinfo.debug_addr.offset;
    ntdll_info.size_debug_addr = ntdll->dwinfo.debug_addr.size;
    ntdll_info.offset_debug_str_offsets = ntdll->dwinfo.debug_str_offsets.offset;
    ntdll_info.size_debug_str_offsets = ntdll->dwinfo.debug_str_offsets.size;

    auto ntdll_info_addr = ctx_addr - sizeof(ntdll_info);

    if (!copy_to((void*)ntdll_info_addr, &ntdll_info, sizeof(ntdll_info))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    auto& sect = img->get();

    img_info.base = (void*)img_addr;
    img_info.offset_debug_info = sect.dwinfo.debug_info.offset;
    img_info.size_debug_info = sect.dwinfo.debug_info.size;
    img_info.offset_debug_abbrev = sect.dwinfo.debug_abbrev.offset;
    img_info.size_debug_abbrev = sect.dwinfo.debug_abbrev.size;
    img_info.offset_debug_rnglists = sect.dwinfo.debug_rnglists.offset;
    img_info.size_debug_rnglists = sect.dwinfo.debug_rnglists.size;
    img_info.offset_debug_str = sect.dwinfo.debug_str.offset;
    img_info.size_debug_str = sect.dwinfo.debug_str.size;
    img_info.offset_debug_addr = sect.dwinfo.debug_addr.offset;
    img_info.size_debug_addr = sect.dwinfo.debug_addr.size;
    img_info.offset_debug_str_offsets = sect.dwinfo.debug_str_offsets.offset;
    img_info.size_debug_str_offsets = sect.dwinfo.debug_str_offsets.size;

    auto img_info_addr = ntdll_info_addr - sizeof(img_info);

    if (!copy_to((void*)img_info_addr, &img_info, sizeof(img_info))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    memset(&t.context, 0, sizeof(CONTEXT));
    memset(&t.syscall_context, 0, sizeof(CONTEXT));

    t.context.EFlags |= 0x202; // IF, reserved flag
    t.context.SegCs = user_CS;
    t.context.SegSs = user_DS;
    t.context.SegEs = user_DS;
    t.context.SegFs = user_DS;
    t.context.SegGs = user_DS;
    t.context.Rip = ntdll_addr + mn_start_thread;
    t.context.Rcx = ntdll_info_addr;
    t.context.Rdx = img_info_addr;
    t.context.R8 = ctx_addr;
    t.context.Rsp = (uintptr_t)img_info_addr - 0x20; // 0x20 for MS ABI

    t.kernel_mode = false;

    t.suspend_count = create_suspended ? 1 : 0;
    t.status = thread_status::runnable;

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

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

    client_id.UniqueProcess = 0; // FIXME - PID
    client_id.UniqueThread = tid;

    // FIXME - copy initial_teb into TEB
    // FIXME - copy client_id into TEB

    if (thread_handle) {
        auto& p = *get_current_thread()->proc;

        handle_t h;

        auto status = p.add_handle(t, 0, true, h); // FIXME - access mask
        if (!success(status))
            return status;

        thread_handle->get() = h;
    }

    return nt_status::SUCCESS;
}
