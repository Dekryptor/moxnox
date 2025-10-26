module;

#include <span>
#include <optional>
#include <string_view>
#include <functional>
#include <variant>
#include <mutex>
#include "rangelock.h"
#include "thread.h"
#include "process.h"
#include "file.h"
#include "mapping.h"
#include "section2.h"
#include "assert.h"

#ifdef __aarch64__
#include "mem-aarch64.h"
#else
#include "mem-x86.h"
#endif

export module process;

import print;
import bsod;
import mem;
import config;
import list;
import vfs;
import string;
import interrupts;
import visit;
import unicode;
import section;
import thread;
import rangelock;
import rwsem;
import scheduler;
import smp;
import disable_interrupts;
import page_cache;
import objects;
import inode;
import cpu_block;
import ntdll;
import pe;

#ifndef __aarch64__
import apic;
#endif

using namespace std;

struct handle_obj {
    rbtree_node<handle_t> tree;
    object* obj;
    uint32_t access_mask;
};

static rwsem ipi_lock;

// tlb.S
extern "C" uint64_t tlb_va, tlb_num_pages;
extern "C" atomic<uint32_t> tlb_cpus_left;

static const uint64_t MAX_USERSPACE_ADDRESS = 0xffffffffffff;

extern "C++" {
    section* ntdll = nullptr;
}

static uintptr_t mn_start_thread;

static_assert(sizeof(TEB) == 0x1818);
static_assert(sizeof(PEB) == 0x388);
static_assert(sizeof(RTL_USER_PROCESS_PARAMETERS) == 0x440);

static const size_t MAX_PATH = 260;

static const uint16_t user_CS = 0x3b;
static const uint16_t user_DS = 0x33;

static atomic<uintptr_t> next_pid = 1;

static spinlock process_list_lock;
static rbtree_node<uintptr_t>* process_tree = nullptr;

#ifndef __aarch64__
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
#endif

#ifdef __aarch64__
using pte_type = ARM64_HARDWARE_PTE;
#else
using pte_type = HARDWARE_PTE;
#endif

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

static bool allocate_proc_params(process& proc, HANDLE stdinh, HANDLE stdouth,
                                 HANDLE stderrh, HANDLE curdirh,
                                 string_view curdir, string_view fn,
                                 u16string_view cmdline) {
    // FIXME - also include space for environment, etc.

    auto curdir_len = dos_path_len(curdir);
    auto fn_len = dos_path_len(fn);

    size_t size;

    size = sizeof(RTL_USER_PROCESS_PARAMETERS);
    size += MAX_PATH * sizeof(char16_t);
    size += (fn_len + 1) * sizeof(char16_t);
    size += (cmdline.size() + 1) * sizeof(char16_t);

    if (auto status = proc.allocate_vm((void**)&proc.proc_params, 0, &size, MEM_COMMIT | MEM_RESERVE, mapping_type::read_write); !success(status)) {
        print("process::allocate_vm returned {}\n", status);
        return false;
    }

    // auto num_pages = bytes_to_pages(size);s

    auto pm = find_mapping(proc, (uintptr_t)proc.proc_params);
    if (!pm) {
        print("allocate_proc_params: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;

    auto ks_proc_params = (RTL_USER_PROCESS_PARAMETERS*)malloc(sizeof(RTL_USER_PROCESS_PARAMETERS));
    if (!ks_proc_params) {
        print("malloc failed\n");
        m.release();
        return false;
    }

    ks_proc_params->StandardInput = stdinh;
    ks_proc_params->StandardOutput = stdouth;
    ks_proc_params->StandardError = stderrh;
    ks_proc_params->CurrentDirectory.Handle = curdirh;

    ks_proc_params->CurrentDirectory.DosPath.Length = (uint16_t)(min(MAX_PATH, curdir_len) * sizeof(char16_t));
    ks_proc_params->CurrentDirectory.DosPath.MaximumLength = MAX_PATH * sizeof(char16_t);
    ks_proc_params->CurrentDirectory.DosPath.Buffer = (char16_t*)((uint8_t*)proc.proc_params + sizeof(RTL_USER_PROCESS_PARAMETERS));

    ks_proc_params->ImagePathName.Length = (uint16_t)(fn_len * sizeof(char16_t));
    ks_proc_params->ImagePathName.MaximumLength = ks_proc_params->ImagePathName.Length + sizeof(char16_t);
    ks_proc_params->ImagePathName.Buffer = ks_proc_params->CurrentDirectory.DosPath.Buffer +
        (ks_proc_params->CurrentDirectory.DosPath.MaximumLength / sizeof(char16_t));

    ks_proc_params->CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
    ks_proc_params->CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
    ks_proc_params->CommandLine.Buffer = ks_proc_params->ImagePathName.Buffer +
        (ks_proc_params->ImagePathName.MaximumLength / sizeof(char16_t));

    auto dos_path = (char16_t*)malloc(ks_proc_params->CurrentDirectory.DosPath.MaximumLength);
    if (!dos_path) {
        print("malloc failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    create_dos_path(span(dos_path, ks_proc_params->CurrentDirectory.DosPath.MaximumLength / sizeof(char16_t)), curdir);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params->CurrentDirectory.DosPath.Buffer - m.address),
                   dos_path, ks_proc_params->CurrentDirectory.DosPath.Length)) {
        print("mapping::copy_to failed\n");
        m.release();
        free(dos_path);
        free(ks_proc_params);
        return false;
    }

    free(dos_path);

    auto fn_path = (char16_t*)malloc(ks_proc_params->ImagePathName.MaximumLength);
    if (!fn_path) {
        print("malloc failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    create_dos_path(span(fn_path, ks_proc_params->ImagePathName.MaximumLength / sizeof(char16_t)), fn);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params->ImagePathName.Buffer - m.address),
                   fn_path, ks_proc_params->ImagePathName.Length)) {
        print("mapping::copy_to failed\n");
        m.release();
        free(fn_path);
        free(ks_proc_params);
        return false;
    }

    free(fn_path);

    if (!m.copy_to((uint32_t)((uintptr_t)ks_proc_params->CommandLine.Buffer - m.address),
                   cmdline.data(), ks_proc_params->CommandLine.Length)) {
        print("mapping::copy_to failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    if (!m.copy_to((uint32_t)((uintptr_t)proc.proc_params - m.address),
                   ks_proc_params, sizeof(RTL_USER_PROCESS_PARAMETERS))) {
        print("mapping::copy_to failed\n");
        m.release();
        free(ks_proc_params);
        return false;
    }

    free(ks_proc_params);

    m.release();

    return true;
}

static bool allocate_peb(process& proc, HANDLE stdinh, HANDLE stdouth,
                         HANDLE stderrh, HANDLE curdirh, string_view curdir,
                         string_view fn, u16string_view cmdline) {
    static_assert(sizeof(PEB) <= PAGE_SIZE);

    size_t peb_size = sizeof(PEB);

    if (!allocate_proc_params(proc, stdinh, stdouth, stderrh, curdirh, curdir,
                              fn, cmdline)) {
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

    auto pa = allocate_folio(0);
    if (!pa) {
        print("allocate_folio failed\n");
        m.release();
        return false;
    }

    memset(phys_address(pa), 0, PAGE_SIZE); // FIXME - get zeroed page

    auto& ks_peb = *(PEB*)phys_address(pa);

    ks_peb.ImageBaseAddress = (void*)proc.img_addr;
    ks_peb.ProcessParameters = proc.proc_params;

    if (!m.copy_to((uint32_t)((uintptr_t)proc.peb - m.address), &ks_peb,
                   sizeof(PEB))) {
        print("mapping::copy_to failed\n");
        m.release();
        free_folio(pa);
        return false;
    }

    m.release();

    free_folio(pa);

    return true;
}

export  object_ref create_process(string_view fn, string_view curdir,
                                  u16string_view cmdline, object_ref& stdinobj,
                                  object_ref& stdoutobj, object_ref& stderrobj) {
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

    if (!pptr->initialized) {
        print("process::process failed\n");
        delete pptr;
        return {};
    }

    auto& p = *pptr;

    p.tree.key = next_pid.fetch_add(1);

    // create section for image

    object_ref f(*new file(img_inode, 0));
    object_ref imgh;

    status = create_section(static_cast<file&>(f.get()), true, 0,
                            PAGE_READWRITE, imgh);
    if (!success(status)) {
        print("create_section returned {}\n", status);
        p.release();
        return {};
    }

    p.img = static_cast<section&>(imgh.get());

#ifdef __aarch64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_ARM64;
#else
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_AMD64;
#endif

    if (p.img->get().machine != this_arch) {
        print("invalid architecture {:x}\n", p.img->get().machine);
        p.release();
        return {};
    }

#ifdef __aarch64__
    if (!p.allocate_ttbr0()) {
        print("process::allocate_ttbr0 failed\n");
        p.release();
        return {};
    }
#else
    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return {};
    }
#endif

    p.img->get().acquire();

    imgh.reset();

    auto& img = p.img->get();

    p.img_addr = p.map_section(img, img.preferred_base + img.size > MAX_USERSPACE_ADDRESS ? 0 : img.preferred_base,
                               nullopt, mapping_type::read_only, true, nullptr);
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

    p.ntdll_addr = p.map_section(*ntdll, ntdll->preferred_base,
                                 nullopt,  mapping_type::read_only, true,
                                 nullptr);
    if (!p.ntdll_addr) {
        print("process::map_section failed\n");
        p.release();
        return {};
    }

    // FIXME - map time page into process

    handle_t stdinh = 0, stdouth = 0, stderrh = 0;

    if (stdinobj) {
        status = p.add_handle(stdinobj.get(), FILE_READ_DATA, true, stdinh);
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    if (stdoutobj) {
        status = p.add_handle(stdoutobj.get(), FILE_WRITE_DATA, true, stdouth);
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    if (stderrobj) {
        status = p.add_handle(stderrobj.get(), FILE_WRITE_DATA, true, stderrh);
        if (!success(status)) {
            print("add_handle failed\n");
            p.release();
            return {}; // FIXME - return status
        }
    }

    auto curdir_file = new file(curdir_inode, 0);
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

    if (!allocate_peb(p, stdinh, stdouth, stderrh, curdirh, curdir, fn, cmdline)) {
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

    uintptr_t entry_point_ctx_addr = (uintptr_t)stackaddr + stack_size - sizeof(CONTEXT);

#ifdef __aarch64__
    entry_point_ctx_addr -= 0x10; // 0x10 for red zone
#else
    entry_point_ctx_addr -= 0x20; // 0x20 for MS ABI
#endif

    memset(&entry_point_context, 0, sizeof(entry_point_context));

#ifdef __aarch64__
    entry_point_context.Pc = p.img_addr + img.entry_point;
    entry_point_context.Sp = entry_point_ctx_addr + sizeof(CONTEXT);
#else
    entry_point_context.Rip = p.img_addr + img.entry_point;
    entry_point_context.Rsp = entry_point_ctx_addr + sizeof(CONTEXT);
#endif

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

    {
        lock_guard lock(process_list_lock);
        insert_into_rbtree(process_tree, p.tree);
    }

    p.added_to_tree = true;

    return p;
}

extern "C++"
process::process() : sync_object(object_type::process, false) {
    auto s = new section;
    if (!s) {
        print("new failed\n");
        return;
    }

    s->type = section_type::anonymous;
    s->num_pages = 0;
    s->size = 0;
    s->page_protection = PAGE_EXECUTE_READWRITE;

    s->anon_cache = new anon_section_cache;
    if (!s->anon_cache) {
        print("new failed\n");
        s->release();
        return;
    }

    vm_sect = *s;

    initialized = true;
}

#ifndef __aarch64__
static void free_page_tables(process& proc) {
    if (proc.cr3 == 0)
        return;

    // FIXME - handle huge and gigantic pages

    auto ps1 = span((HARDWARE_PTE*)phys_address(proc.cr3), 256); // not higher half

    for (auto& p1 : ps1) {
        if (!p1.p)
            continue;

        auto ps2 = span((HARDWARE_PTE*)phys_address(p1.pfn << PAGE_SHIFT), 512);

        for (auto& p2 : ps2) {
            if (!p2.p)
                continue;

            auto ps3 = span((HARDWARE_PTE*)phys_address(p2.pfn << PAGE_SHIFT), 512);

            for (auto& p3 : ps3) {
                if (!p3.p)
                    continue;

                free_folio(p3.pfn << PAGE_SHIFT);
            }

            free_folio(p2.pfn << PAGE_SHIFT);
        }

        free_folio(p1.pfn << PAGE_SHIFT);
    }

    free_folio(proc.cr3);
}
#endif

extern "C++"
process::~process() {
    // FIXME - race condition here with NtOpenProcess etc.
    if (added_to_tree) {
        lock_guard lock(process_list_lock);
        remove_from_rbtree(process_tree, tree);
    }

    while (true) {
        mapping* m;

        {
            lock_guard lock(mappings_lock);

            if (is_list_empty(mappings))
                break;

            m = &containing_record<mapping>(mappings.next);

            remove_entry_list(m->le);
        }

        m->release();
    }

    if (img)
        img->get().release();

#ifndef __aarch64__ // FIXME
    free_page_tables(*this);
#endif
}

#ifdef __aarch64__
static bool set_page_protection(process& proc, uint64_t va, uint32_t num_pages,
                                enum mapping_type protect, bool cow,
                                bool reserve) {
    span<ARM64_HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((ARM64_HARDWARE_PTE*)phys_address(proc.ttbr0), 512);

    auto redo_level = [&](uint8_t level) -> bool {
        if (!pt[level][index[level]].valid) {
            uint64_t pa = allocate_page();
            if (pa == 0)
                return false;

            pt[level][index[level]].valid = 1;
            pt[level][index[level]].table_entry = 1;
            pt[level][index[level]].AP = ARM64_PTE_AP_EL0RW;
            pt[level][index[level]].SH = 3; // inner shareable
            pt[level][index[level]].AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            pt[level][index[level]].pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

            memset(phys_address(pa), 0, PAGE_SIZE);
        }

        pt[level - 1] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);

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

        if (pte.valid && pte.AP == ARM64_PTE_AP_EL0RW && pte.cow) {
            // don't clear COW flag if already COWed

            // FIXME - should be able to make COWed page readonly?

            switch (protect) {
                case mapping_type::read_execute:
                case mapping_type::read_write_execute:
                    pte.UXN = 0;
                    break;

                default:
                    pte.UXN = 1;
                    break;
            }
        } else {
            pte.reserved = reserve ? 1 : 0;

            switch (protect) {
                case mapping_type::read_write:
                    pte.UXN = 1;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                case mapping_type::read_execute:
                    pte.UXN = 0;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;

                case mapping_type::read_write_execute:
                    pte.UXN = 0;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                default:
                    pte.UXN = 1;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;
            }
        }
    }

    __asm __volatile (
        "tlbi vae1, %x[res]\n"
        "dsb ish\n"
        "isb\n"
        :
        : [res] "r" (va >> PAGE_SHIFT)
    );

    return true;
}
#else
static bool set_page_protection(process& proc, uint64_t va, uint32_t num_pages,
                                enum mapping_type protect, bool cow,
                                bool reserve) {
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

        if (pte.p && pte.w && pte.cow) {
            // don't clear COW flag if already COWed

            // FIXME - should be able to make COWed page readonly?

            switch (protect) {
                case mapping_type::read_execute:
                case mapping_type::read_write_execute:
                    pte.xd = 0;
                    break;

                default:
                    pte.xd = 1;
                    break;
            }
        } else {
            pte.u = 1;
            pte.w = 0;
            pte.reserved = reserve ? 1 : 0;

            switch (protect) {
                case mapping_type::read_write:
                    pte.xd = 1;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                case mapping_type::read_execute:
                    pte.xd = 0;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;

                case mapping_type::read_write_execute:
                    pte.xd = 0;
                    pte.allow_write = 1;
                    pte.cow = cow ? 1 : 0;
                    break;

                default:
                    pte.xd = 1;
                    pte.allow_write = 0;
                    pte.cow = 0;
                    break;
            }
        }
    }

    return true;
}
#endif

extern "C++"
uintptr_t process::map_section(section& sect, uintptr_t address,
                               optional<int64_t> section_offset,
                               enum mapping_type protect, bool cow,
                               size_t* view_size) {
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

    if (section_offset.has_value()) {
        if ((uint64_t)*section_offset % PAGE_SIZE) {
            print("process::map_section: invalid offset {:x}\n", *section_offset);
            return 0;
        }

        if (*section_offset >= sect.size) {
            print("process::map_section: offset {:x} >= section size {:x}\n",
                  *section_offset, sect.size);
            return 0;
        }
    }

    auto m = new mapping(*this, sect);
    if (!m) {
        print("new failed\n");
        return 0;
    }

    m->address = address;
    m->offset = section_offset.has_value() ? (uint64_t)*section_offset : 0;
    m->size = (uint32_t)(sect.size - m->offset);

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

        if (view_size)
            *view_size = m->size;
    }

    if (sect.pe_sections) {
        uint64_t addr = 0;

        for (unsigned int i = 0; i < sect.num_pe_sections; i++) {
            auto& s = sect.pe_sections[i];

            if (s.VirtualAddress > addr) {
                if (!set_page_protection(*this, address + addr, (uint32_t)((s.VirtualAddress - addr) >> PAGE_SHIFT),
                                         mapping_type::read_only, false, false)) {
                    return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
                }
            }

            uint64_t num_pages = bytes_to_pages(s.VirtualSize);

            if (!set_page_protection(*this, address + s.VirtualAddress, (uint32_t)num_pages,
                                     characteristics_to_mapping_type(s.Characteristics),
                                     true, false)) {
                return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
            }

            addr = s.VirtualAddress + (num_pages << PAGE_SHIFT);
        }

        if (addr < sect.size) {
            if (!set_page_protection(*this, address + addr, (uint32_t)((sect.size - addr) >> PAGE_SHIFT),
                                     mapping_type::read_only, false, false)) {
                return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
            }
        }
    } else {
        if (!set_page_protection(*this, address, sect.num_pages, protect, cow, false))
            return 0; // FIXME - return nt_status::INSUFFICIENT_RESOURCES
    }

    return address;
}

static bool populate_teb(process& proc, ::thread& t) {
    // only care about first page
    static_assert(offsetof(TEB, NtTib.Self) >> PAGE_SHIFT == 0);
    static_assert(offsetof(TEB, ProcessEnvironmentBlock) >> PAGE_SHIFT == 0);

    auto pm = find_mapping(proc, (uintptr_t)t.teb);
    if (!pm) {
        print("populate_teb: find_mapping failed\n");
        return false;
    }

    auto& m = *pm;

    auto pa = allocate_folio(0);
    if (!pa) {
        print("allocate_folio failed\n");
        m.release();
        return false;
    }

    memset(phys_address(pa), 0, PAGE_SIZE); // FIXME - get zeroed page

    auto& teb = *(TEB*)phys_address(pa);

    teb.NtTib.Self = &t.teb->NtTib;
    // FIXME - StackBase
    // FIXME - StackLimit
    // FIXME - ClientId
    teb.ProcessEnvironmentBlock = proc.peb;

    if (!m.copy_to((uint32_t)((uintptr_t)t.teb - m.address), &teb,
                   PAGE_SIZE)) {
        print("mapping::copy_to failed\n");
        m.release();
        free_folio(pa);
        return false;
    }

    m.release();

    free_folio(pa);

    return true;
}

#ifdef __aarch64__
static ARM64_HARDWARE_PTE* find_pte(process& proc, uintptr_t va) {
    span<ARM64_HARDWARE_PTE> pt[4];

    pt[3] = span((ARM64_HARDWARE_PTE*)phys_address(proc.ttbr0), 512);

    if (!pt[3][(va & 0xff8000000000) >> 39].valid)
        return nullptr;

    pt[2] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[3][(va & 0xff8000000000) >> 39].pfn) << PAGE_SHIFT)), 512);

    if (!pt[2][(va & 0x7fc0000000) >> 30].valid)
        return nullptr;

    pt[1] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[2][(va & 0x7fc0000000) >> 30].pfn) << PAGE_SHIFT)), 512);

    if (!pt[1][(va & 0x3fe00000) >> 21].valid)
        return nullptr;

    pt[0] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[1][(va & 0x3fe00000) >> 21].pfn) << PAGE_SHIFT)), 512);

    return &pt[0][(va & 0x1ff000) >> 12];
}
#else
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
#endif

#ifdef __llvm__ // work around Clang *really* wanting exceptions to be enabled
extern "C++"
exception::~exception() {
}

extern "C++"
const char* exception::what() const noexcept {
    return nullptr;
}
#endif

#ifndef __aarch64__
static void send_tlb_ipi(uint8_t cpu_no) {
    tlb_cpus_left++;

    auto icr2 = (volatile uint32_t*)&local_apic[0x310];

    *icr2 = (*icr2 & 0x00ffffff) | ((uint32_t)cpu_no << 24);

    auto icr1 = (volatile uint32_t*)&local_apic[0x300];

    *icr1 = invalidate_tlb_interrupt;

    while (*icr1 & (1 << 12)) { } // wait to clear
}

static void send_page_tlb_ipi(process& proc, uint64_t va) {
    auto cpus = proc.running_cpus.load();

    if (cpus == 0)
        return;

    {
        lock_guard lock(ipi_lock);

        disable_preempt();

        tlb_va = va;
        tlb_num_pages = 1;

        tlb_cpus_left = 1;

        for (uint8_t i = 0; i < num_cpus; i++) {
            if (!(cpus & 1 << i))
                continue;

            send_tlb_ipi(i);
        }

        tlb_cpus_left--;

        while (tlb_cpus_left > 0) { }

        enable_preempt();
    }
}
#endif

static bool handle_page_fault2(process& p, uintptr_t address, pte_type& pte) {
    rangelock_req rlr(p.rl, address >> PAGE_SHIFT, 1);

    if (!pte.allow_write)
        return false;

#ifdef __aarch64__
    if (pte.AP != ARM64_PTE_AP_EL0RO)
        return false;
#else
    if (pte.w)
        return false;
#endif

    // mark cache as dirty

    auto m = find_mapping(p, address);

    if (!m)
        return false;

    uint64_t page_num = (address - m->address) >> PAGE_SHIFT;
    uint64_t sect_page_num = page_num + (m->offset >> PAGE_SHIFT);
    bool send_ipi = false;

    if (pte.cow) {
        auto pfn = allocate_folio(0) >> PAGE_SHIFT;
        if (pfn == 0) {
            print("allocate_folio failed\n");
            m->release();
            return false;
        }

        memcpy(phys_address(pfn << PAGE_SHIFT),
                phys_address(pte.pfn << PAGE_SHIFT),
                PAGE_SIZE);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pte.pfn = pfn;
#pragma GCC diagnostic pop
    } else {
        auto ret = visit([&](auto* ptr) {
            auto& cache = *ptr;
            uint64_t read_pages;

            auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                        read_pages);
            if (!success(status)) {
                print("read_into_cache_and_pin returned {}\n", status);
                return false;
            }

            rangelock_req rlr(cache.rl, sect_page_num, 1);

            page_cache_entry* p;

            {
                lock_guard lg(cache.cache_lock);

                p = cache.find_page_cache_entry(sect_page_num);
            }

            if (!p->dirty) {
                assert(p->pfn != 0);

                auto new_pfn = cow_folio(p->pfn << PAGE_SHIFT) >> PAGE_SHIFT;
                if (new_pfn == 0) {
                    print("out of memory\n");
                    cache.unpin(sect_page_num, 1);
                    m->release();
                    return false;
                }

                if (new_pfn != p->pfn) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    p->pfn = new_pfn;
#pragma GCC diagnostic pop

                    send_ipi = true;
                }

                p->dirty = 1;
                cache.mark_dirty();
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            pte.pfn = p->pfn;
#pragma GCC diagnostic pop

            cache.unpin(sect_page_num, 1); // FIXME?

            return true;
        }, m->sect.get_cache());

        if (!ret) {
            m->release();
            return false;
        }
    }

    m->release();

#ifdef __aarch64__
    pte.AP = ARM64_PTE_AP_EL0RW;

    __asm __volatile (
        "tlbi vae1, %x[res]\n"
        "dsb ish\n"
        "isb\n"
        :
        : [res] "r" (address >> PAGE_SHIFT)
    );
#else
    pte.w = 1;
#endif

#ifndef __aarch64__ // FIXME
    if (send_ipi)
        send_page_tlb_ipi(p, address);
#endif

    return true;
}

#ifdef __aarch64__
extern "C++"
bool process::handle_page_fault(uintptr_t address, uint64_t esr_value) {
    ARM64_ESR esr;

    esr.value = esr_value;

    address &= ~(PAGE_SIZE - 1);

    auto pte = find_pte(*this, address);

    if (pte && pte->valid) {
        // FIXME - NX violations

        // don't handle if kernel-only page
        if (pte->AP == ARM64_PTE_AP_EL1RW || pte->AP == ARM64_PTE_AP_EL1RO)
            return false;

        return handle_page_fault2(*this, address, *pte);
    }

    if (pte && pte->reserved)
        return false;

    auto m = find_mapping(*this, address);
    if (!m)
        return false;

    auto ret = m->map((uint32_t)(address - m->address), address);

    m->release();

    return ret;
}
#else
extern "C++"
bool process::handle_page_fault(uintptr_t address, uint64_t error_code) {
    page_fault_error_code ec;

    ec.value = error_code;

    address &= ~(PAGE_SIZE - 1);

    auto pte = find_pte(*this, address);

    if (ec.present) {
        if (ec.instruction_fetch) // NX violation
            return false;

        if (!pte)
            return false;

        return handle_page_fault2(*this, address, *pte);
    }

    if (pte && pte->reserved)
        return false;

    auto m = find_mapping(*this, address);
    if (!m)
        return false;

    auto ret = m->map((uint32_t)(address - m->address), address);

    m->release();

    return ret;
}
#endif

#ifdef __aarch64__
extern "C++"
bool process::allocate_ttbr0() {
    ttbr0 = allocate_folio(0);
    if (!ttbr0) {
        print("allocate_folio failed\n");
        return false;
    }

    memset(phys_address(ttbr0), 0, PAGE_SIZE);

    return true;
}
#else
extern "C++"
bool process::allocate_cr3() {
    auto kernel_cr3 = get_cpu_block()->kernel_cr3;

    cr3 = allocate_folio(0);
    if (!cr3) {
        print("allocate_folio failed\n");
        return false;
    }

    auto va = phys_address(cr3);

    // copy higher half mappings

    memset((void*)va, 0, PAGE_SIZE >> 1);
    memcpy((uint8_t*)va + (PAGE_SIZE >> 1),
           (uint8_t*)phys_address(kernel_cr3) + (PAGE_SIZE >> 1),
           PAGE_SIZE >> 1);

    return true;
}
#endif

extern "C++"
bool process::copy_from(void* ks_addr, const void* us_addr, size_t size) {
    while (true) {
        auto m = find_mapping(*this, (uintptr_t)us_addr);
        if (!m)
            return false;

        auto to_copy = min(size, m->address + m->size - (uintptr_t)us_addr);

        if (!m->copy_from((uint32_t)((uintptr_t)us_addr - m->address), ks_addr, (uint32_t)to_copy)) {
            m->release();
            return false;
        }

        m->release();

        if (size == to_copy)
            return true;

        us_addr = (uint8_t*)us_addr + to_copy;
        ks_addr = (uint8_t*)ks_addr + to_copy;
        size -= to_copy;
    }

    return false;
}

extern "C++"
bool process::copy_to(void* us_addr, const void* ks_addr, size_t size) {
    while (true) {
        auto m = find_mapping(*this, (uintptr_t)us_addr);
        if (!m)
            return false;

        auto to_copy = min(size, m->address + m->size - (uintptr_t)us_addr);

        if (!m->copy_to((uint32_t)((uintptr_t)us_addr - m->address), ks_addr, (uint32_t)to_copy)) {
            m->release();
            return false;
        }

        m->release();

        if (size == to_copy)
            return true;

        us_addr = (uint8_t*)us_addr + to_copy;
        ks_addr = (uint8_t*)ks_addr + to_copy;
        size -= to_copy;
    }

    return false;
}

export void load_ntdll() {
    nt_status status;
    object_ref inode_ref;
    unsigned int create_result;

    status = open_inode("/ntdll.dll", nullopt, inode_ref, FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE, create_result);
    if (!success(status))
        bsod_format("open_inode returned {} for ntdll.dll", status);

    if (inode_ref.get().type != object_type::inode)
        bsod("open_inode returned something other than an inode");

    object_ref f(*new file(inode_ref.get(), 0));
    object_ref ntdllh;

    status = create_section(static_cast<file&>(f.get()), true, 0,
                            PAGE_READWRITE, ntdllh);
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

static void free_handle(rbtree_node<handle_t>* tree) {
    auto& ho = rbtree_containing_record<handle_obj>(tree);

    // FIXME - is recursion going to be a problem here?

    if (tree->left)
        free_handle(tree->left);

    if (tree->right)
        free_handle(tree->right);

    free(&ho);
}

extern "C++"
void process::terminate() {
    // wake up threads waiting for process to terminate

    {
        sync_lock sl;
        lock_guard lock(sl);

        signalled = true;

        wake_up_waiters(waiters);
    }

    // close open handles

    {
        lock_guard lock(handle_lock);

        if (handles) {
            auto n = get_tree_minimum(handles);

            while (n) {
                auto& ho = rbtree_containing_record<handle_obj>(n);

                handle_closed(*ho.obj);
                object_release(*ho.obj);

                n = next_rbtree_node(*n);
            }

            free_handle(handles);
            handles = nullptr;
        }
    }
}

extern "C"
NTSTATUS NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status) {
    object_ref obj;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        auto p = t->proc;
        p->acquire();

        obj = object_ref{*p};
    } else {
        obj = deref_handle(process_handle);
        if (!obj)
            return nt_status::INVALID_HANDLE;

        if (obj.get().type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights
    }

    auto& p = static_cast<process&>(obj.get());

    {
        lock_guard lock(p.threads_lock);

        if (p.dying)
            return nt_status::SUCCESS;

        p.dying = true;
    }

    while (true) {
        ::thread* t;

        {
            lock_guard lock(p.threads_lock);

            if (is_list_empty(p.threads))
                break;

            t = (::thread*)((uint8_t*)p.threads.next - offsetof(::thread, process_le));

            t->exit_status = exit_status;
            terminate_thread(*t, false);
            remove_entry_list(t->process_le);
        }

        // t->release(); // FIXME
    }

    p.exit_status = exit_status;
    p.terminate();

    return nt_status::SUCCESS;
}

extern "C++"
void process::resume() {
    if (--suspend_count != 0)
        return;

    sync_lock sl;

    lock_guard lock1(sl);
    lock_guard lock2(threads_lock);

    auto le = threads.next;

    while (le != &threads) {
        auto& t = *(::thread*)((uint8_t*)le - offsetof(::thread, process_le));

        t.suspend_count--;

        le = le->next;
    }
}

extern "C++"
void process::suspend() {
    if (suspend_count++ != 0)
        return;

    sync_lock sl;

    lock_guard lock1(sl);
    lock_guard lock2(threads_lock);

    auto le = threads.next;

    while (le != &threads) {
        auto& t = *(::thread*)((uint8_t*)le - offsetof(::thread, process_le));

        t.suspend_count++;

        le = le->next;
    }
}

extern "C"
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

extern "C"
NTSTATUS NtSuspendProcess(HANDLE proc_handle) {
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

    p->suspend();

    return nt_status::SUCCESS;
}

extern "C++"
nt_status process::add_handle(object& obj, uint32_t access_mask,
                              bool allow_shared, handle_t& h) {
    // FIXME - reusing handles?

    if (obj.handle_count.fetch_add(1) > 0 && !allow_shared) {
        obj.handle_count--;
        return nt_status::SHARING_VIOLATION;
    }

    auto ho = (handle_obj*)malloc(sizeof(handle_obj));
    if (!ho)
        return nt_status::INSUFFICIENT_RESOURCES;

    object_acquire(obj);

    {
        lock_guard lock(handle_lock);

        h = ++last_handle;

        ho->tree.key = h;
        ho->obj = &obj;
        ho->access_mask = access_mask;

        insert_into_rbtree(handles, ho->tree);
    }

    return nt_status::SUCCESS;
}

extern "C++"
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

            auto to_commit = min(size, m->address + m->size - addr);

            if (!set_page_protection(*this, addr, (uint32_t)(to_commit >> PAGE_SHIFT), protect,
                                     false, false)) {
                m->release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            if (to_commit == size) {
                m->release();
                return nt_status::SUCCESS;
            }

            addr += to_commit;
            size -= to_commit;

            m->release();
        }

        return nt_status::NO_MEMORY;
    }

    auto& sect = static_cast<section&>(vm_sect.get());

    auto m = new mapping(*this, sect);
    if (!m) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    m->size = (uint32_t)size;

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

        m->offset = addr;
        m->address = addr;
    }

    if (!set_page_protection(*this, addr, num_pages, protect, false,
                             !(allocation_type & MEM_COMMIT))) {
        print("set_page_protection failed\n");
        {
            lock_guard lock(mappings_lock);
            remove_entry_list(m->le);
            m->release();
        }
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    *base_address = (void*)addr;
    *region_size = size;

    return nt_status::SUCCESS;
}

extern "C"
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

#ifndef __aarch64__
static void unmap_proc_pages(process& proc, uint64_t va, uint64_t num_pages) {
    span<HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    auto redo_level = [&](uint8_t level) {
        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    };

    for (uint8_t level = 3; level > 0; level--) {
        redo_level(level);
    }

    for (uint64_t i = 0; i < num_pages; i++) {
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

        // free COWed folios
        if (pte.p && pte.w && pte.cow)
            free_folio(pte.pfn << PAGE_SHIFT);

        pte.p = 0;
    }
}
#endif

extern "C++"
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

extern "C"
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

extern "C++"
mapping::mapping(process& proc, section& sect) : proc(proc), sect(sect) {
    sect.acquire();

    visit([&](auto* ptr) {
        auto& cache = *ptr;

        {
            lock_guard lg(cache.mappings_lock);
            insert_tail_list(cache.mappings, cache_le);
        }
    }, sect.get_cache());
}

extern "C++"
mapping::~mapping() {
    sect.release();

    // FIXME - send IPI to unmap on other CPUs by clearing TLB

#ifndef __aarch64__
    unmap_proc_pages(proc, address, size >> PAGE_SHIFT);
#endif

    visit([&](auto* ptr) {
        auto& cache = *ptr;

        {
            lock_guard lg(cache.mappings_lock);
            remove_entry_list(cache_le);
        }
    }, sect.get_cache());
}

extern "C++"
bool mapping::map(uint32_t mapping_offset, uint64_t va) {
    uint64_t page_num = mapping_offset >> PAGE_SHIFT;
    uint64_t sect_page_num = page_num + (offset >> PAGE_SHIFT);

    return visit([&](auto* ptr) {
        uint64_t read_pages;
        auto& cache = *ptr;

        auto status = cache.read_into_cache_and_pin(sect_page_num, 1, read_pages);
        if (!success(status)) {
            print("page_cache::read_into_cache_and_pin returned {}\n", status);
            return false;
        }

        auto pte = find_pte(proc, va);
        if (!pte) {
            cache.unpin(sect_page_num, 1);
            return false;
        }

        {
            page_cache_entry* p;
            rangelock_req rlr(cache.rl, sect_page_num, 1);

            {
                lock_guard lg(cache.cache_lock);
                p = cache.find_page_cache_entry(sect_page_num);
            }

#ifdef __aarch64__
            pte->pfn = p->pfn;
            pte->AP = ARM64_PTE_AP_EL0RO;
            pte->table_entry = 1;
            pte->SH = 3; // inner shareable
            pte->AF = 1;
            pte->valid = 1;
#else
            pte->pfn = p->pfn & 0x7ffffffffffff;
            pte->p = 1;
#endif
        }

        cache.unpin(sect_page_num, 1);

        return true;
    }, sect.get_cache());
}

extern "C++"
bool mapping::copy_from(uint32_t offset, void* ks_addr, uint32_t size) {
    uint64_t page_num = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + size + PAGE_SIZE - 1) >> PAGE_SHIFT) - page_num;
    uint64_t page_offset = offset & (PAGE_SIZE - 1);

    // FIXME - get process range lock

    for (uint64_t i = 0; i < num_pages; i++) {
        auto& pte = *find_pte(proc, address + ((page_num + i) << PAGE_SHIFT));

        auto to_copy = min((uint32_t)(PAGE_SIZE - page_offset), size);

#ifdef __aarch64__
        bool pte_valid = pte.valid;
#else
        bool pte_valid = pte.p;
#endif

        if (pte_valid) {
            memcpy(ks_addr, (uint8_t*)phys_address(pte.pfn << PAGE_SHIFT) + page_offset,
                   to_copy);
        } else {
            page_cache_entry* p;

            uint64_t sect_page_num = page_num + i + (this->offset >> PAGE_SHIFT);

            auto b = visit([&](auto* ptr) {
                uint64_t read_pages;
                auto& cache = *ptr;

                auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                            read_pages);
                if (!success(status)) {
                    print("page_cache::read_into_cache_and_pin returned {}\n",
                          status);
                    return false;
                }

                {
                    lock_guard lg(cache.cache_lock);
                    p = cache.find_page_cache_entry(sect_page_num);
                }

                {
                    rangelock_req rlr(cache.rl, sect_page_num, 1);

                    memcpy(ks_addr, (uint8_t*)get_phys_address(*p) + page_offset,
                        to_copy);
                }

                cache.unpin(sect_page_num, 1);

                return true;
            }, sect.get_cache());

            if (!b)
                return false;
        }

        ks_addr = (uint8_t*)ks_addr + to_copy;
        size -= to_copy;
        page_offset = 0;
    }

    return true;
}

extern "C++"
bool mapping::copy_to(uint32_t offset, const void* ks_addr, uint32_t size) {
    uint64_t page_num = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + size + PAGE_SIZE - 1) >> PAGE_SHIFT) - page_num;
    uint64_t page_offset = offset & (PAGE_SIZE - 1);

    // FIXME - get process range lock

    for (uint64_t i = 0; i < num_pages; i++) {
        auto& pte = *find_pte(proc, address + ((page_num + i) << PAGE_SHIFT));

        auto to_copy = min((uint32_t)(PAGE_SIZE - page_offset), size);

        if (!pte.allow_write)
            return false;

#ifdef __aarch64__
        bool pte_valid = pte.valid;
        bool pte_writable = pte.AP == ARM64_PTE_AP_EL0RW;
#else
        bool pte_valid = pte.p;
        bool pte_writable = pte.w;
#endif

        if (pte_valid && pte_writable) {
            memcpy((uint8_t*)phys_address(pte.pfn << PAGE_SHIFT) + page_offset,
                   ks_addr, to_copy);
        } else if (pte.cow) {
            auto pa = allocate_folio(0);
            if (!pa) {
                print("allocate_folio failed\n");
                return false;
            }

            if (!pte_valid) {
                if (to_copy < PAGE_SIZE) {
                    page_cache_entry* p;
                    uint64_t sect_page_num = page_num + i + (this->offset >> PAGE_SHIFT);

                    auto b = visit([&](auto* ptr) {
                        uint64_t read_pages;
                        auto& cache = *ptr;

                        auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                                    read_pages);
                        if (!success(status)) {
                            print("page_cache::read_into_cache_and_pin returned {}\n",
                                  status);
                            free_folio(pa);
                            return false;
                        }

                        {
                            lock_guard lg(cache.cache_lock);
                            p = cache.find_page_cache_entry(sect_page_num);
                        }

                        {
                            rangelock_req rlr(cache.rl, sect_page_num, 1);

                            memcpy(phys_address(pa), get_phys_address(*p),
                                PAGE_SIZE);
                        }

                        cache.unpin(sect_page_num, 1);

                        return true;
                    }, sect.get_cache());

                    if (!b)
                        return false;
                }
            } else {
                memcpy(phys_address(pa), phys_address(pte.pfn << PAGE_SHIFT),
                       PAGE_SIZE);
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            pte.pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

            memcpy((uint8_t*)phys_address(pte.pfn << PAGE_SHIFT) + page_offset,
                   ks_addr, to_copy);

#ifdef __aarch64__
            pte.AP = ARM64_PTE_AP_EL0RW;

            if (!pte.valid) {
                pte.table_entry = 1;
                pte.SH = 3; // inner shareable
                pte.AF = 1;
                pte.valid = 1;
            }
#else
            pte.w = 1;

            if (!pte.p)
                pte.p = 1;
#endif
        } else {
            uint64_t sect_page_num = page_num + i + (this->offset >> PAGE_SHIFT);
            page_cache_entry* p;

            auto b = visit([&](auto* ptr) {
                auto& cache = *ptr;

                if (to_copy < PAGE_SIZE) {
                    uint64_t read_pages;

                    auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                                read_pages);
                    if (!success(status)) {
                        print("page_cache::read_into_cache_and_pin returned {}\n",
                              status);
                        return false;
                    }
                } else
                    cache.pin(sect_page_num, 1, nullptr);

                {
                    lock_guard lg(cache.cache_lock);
                    p = cache.find_page_cache_entry(sect_page_num);
                }

                bool send_ipi = false;

                {
                    rangelock_req rlr(cache.rl, sect_page_num, 1);

                    if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop

                        if (!p->pfn) {
                            print("allocate_folio failed\n");
                            cache.unpin(sect_page_num, 1);
                            return false;
                        }
                    } else if (!p->dirty) {
                        auto pfn = cow_folio(p->pfn << PAGE_SHIFT) >> PAGE_SHIFT;
                        if (pfn == 0) {
                            print("cow_folio failed\n");
                            cache.unpin(sect_page_num, 1);
                            return false;
                        }

                        send_ipi = p->pfn != pfn;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        p->pfn = pfn;
#pragma GCC diagnostic pop
                    }

                    p->dirty = 1;
                    cache.mark_dirty();

                    memcpy((uint8_t*)get_phys_address(*p) + page_offset,
                        ks_addr, to_copy);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    pte.pfn = p->pfn;
#pragma GCC diagnostic pop
                }

#ifdef __aarch64__
                pte.table_entry = 1;
                pte.AP = ARM64_PTE_AP_EL0RW;
                pte.SH = 3; // inner shareable
                pte.AF = 1;
                pte.valid = 1;
#else
                pte.w = 1;
                pte.p = 1;
#endif

#ifndef __aarch64__
                if (send_ipi)
                    send_page_tlb_ipi(proc, address);
#endif

                cache.unpin(sect_page_num, 1);

                return true;
            }, sect.get_cache());

            if (!b)
                return false;
        }

        ks_addr = (uint8_t*)ks_addr + to_copy;
        size -= to_copy;
        page_offset = 0;
    }

    return true;
}

extern "C++"
object_ref process::deref_handle(handle_t h) {
    if (h == NtCurrentProcess()) {
        this->acquire();
        return *this;
    }

    if (h == 0)
        return nullopt;

    {
        lock_guard lock(handle_lock);

        auto n = find_rbtree_node(handles, h);

        if (!n || n->key != h)
            return nullopt;

        auto& ho = rbtree_containing_record<handle_obj>(n);

        object_acquire(*ho.obj); // needs to be within spinlock

        return *ho.obj;
    }
}

extern "C++"
object_ref process::deref_handle(handle_t h, ACCESS_MASK& access_mask) {
    if (h == NtCurrentProcess()) {
        this->acquire();
        return *this;
    }

    if (h == 0)
        return nullopt;

    {
        lock_guard lock(handle_lock);

        auto n = find_rbtree_node(handles, h);

        if (!n || n->key != h)
            return nullopt;

        auto& ho = rbtree_containing_record<handle_obj>(n);

        object_acquire(*ho.obj); // needs to be within spinlock

        access_mask = ho.access_mask;

        return *ho.obj;
    }
}

#ifndef __aarch64__
static uint32_t get_page_protection(process& proc, uint64_t va) {
    span<HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    for (uint8_t level = 3; level > 0; level--) {
        if (!pt[level][index[level]].p)
            return PAGE_NOACCESS;

        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    }

    auto& pte = pt[0][index[0]];

    if (pte.xd) {
        if (pte.cow)
            return PAGE_WRITECOPY;
        else if (pte.allow_write)
            return PAGE_READWRITE;
        else
            return PAGE_READONLY;
    } else {
        if (pte.cow)
            return PAGE_EXECUTE_WRITECOPY;
        else if (pte.allow_write)
            return PAGE_EXECUTE_READWRITE;
        else
            return PAGE_EXECUTE_READ;
    }
}
#endif

static nt_status protect_vm(process& proc, void** base_address, size_t* region_size,
                            enum mapping_type prot, bool cow, uint32_t& old_type) {
    auto addr = (uintptr_t)*base_address;
    auto addr_end = addr + *region_size;

    addr &= ~(PAGE_SIZE - 1);

    auto pages = bytes_to_pages(addr_end - addr);

    if (pages == 0)
        return nt_status::INVALID_PARAMETER;

    *base_address = (void*)addr;
    *region_size = pages << PAGE_SHIFT;

    bool first = true;

    {
        rangelock_req rlr(proc.rl, addr >> PAGE_SHIFT, pages);

        while (true) {
            auto m = find_mapping(proc, addr);
            if (!m)
                break;

            auto num_pages = min(pages, (uint32_t)((m->address + m->size - addr) >> PAGE_SHIFT));

#ifndef __aarch64__
            if (first)
                old_type = get_page_protection(proc, addr);
#endif

            if (m->sect.type != section_type::image) {
                if (prot == mapping_type::read_write_execute ||
                    prot == mapping_type::read_execute) {
                    switch (m->sect.page_protection) {
                        case PAGE_EXECUTE_READ:
                        case PAGE_EXECUTE_WRITECOPY:
                        case PAGE_EXECUTE_READWRITE:
                        case PAGE_EXECUTE:
                            break;

                        default:
                            return nt_status::ACCESS_DENIED;
                    }
                }

                if (!cow && (prot == mapping_type::read_write_execute ||
                             prot == mapping_type::read_write)) {
                    switch (m->sect.page_protection) {
                        case PAGE_READWRITE:
                        case PAGE_EXECUTE_READWRITE:
                            break;

                        default:
                            return nt_status::ACCESS_DENIED;
                    }
                }
            }

            if (!set_page_protection(proc, addr, num_pages, prot, cow, false)) {
                m->release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            m->release();

            first = false;

            pages -= num_pages;

            if (pages == 0)
                break;

            addr += num_pages << PAGE_SHIFT;
        }
    }

    if (pages != 0)
        return nt_status::NO_MEMORY;

    // FIXME - if mappings changed and process running on other cores, send IPI to clear TLB

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtProtectVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                uint32_t new_prot, uint32_t* old_prot) {
    enum mapping_type prot;
    object_ref objrw;
    bool cow;
    process* p;

    switch (new_prot) {
        case PAGE_NOACCESS:
        case PAGE_GUARD:
            prot = mapping_type::no_access;
            cow = false;
            break;

        case PAGE_READONLY:
            prot = mapping_type::read_only;
            cow = false;
            break;

        case PAGE_READWRITE:
            prot = mapping_type::read_write;
            cow = false;
            break;

        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
            prot = mapping_type::read_execute;
            cow = false;
            break;

        case PAGE_EXECUTE_READWRITE:
            prot = mapping_type::read_write_execute;
            cow = false;
            break;

        case PAGE_WRITECOPY:
            prot = mapping_type::read_write;
            cow = true;
            break;

        case PAGE_EXECUTE_WRITECOPY:
            prot = mapping_type::read_write_execute;
            cow = true;
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

    return protect_vm(*p, base_address, region_size, prot, cow, *old_prot);
}

extern "C++"
NTSTATUS process::close_handle(handle_t h) {
    if (!h)
        return nt_status::INVALID_HANDLE;

    handle_obj* ho;

    {
        lock_guard lock(handle_lock);

        auto n = find_rbtree_node(handles, h);

        if (!n || n->key != h)
            return nt_status::INVALID_HANDLE;

        ho = &rbtree_containing_record<handle_obj>(n);

        remove_from_rbtree(handles, ho->tree);
    }

    handle_closed(*ho->obj);
    object_release(*ho->obj);

    free(ho);

    return nt_status::SUCCESS;
}

extern "C++"
NTSTATUS process::map_view_of_section(section& sect, void** base_address, uintptr_t, size_t,
                                      optional<int64_t> section_offset, size_t& view_size,
                                      enum SECTION_INHERIT, uint32_t,
                                      enum mapping_type protect, bool cow) {
    // FIXME - zero_bits
    // FIXME - commit_size
    // FIXME - view_size
    // FIXME - inherit_disposition
    // FIXME - allocation_type

    auto addr = (uintptr_t)*base_address;

    if (addr & (PAGE_SIZE - 1))
        addr &= ~(PAGE_SIZE - 1);

    addr = map_section(sect, addr, section_offset, protect, cow, &view_size);

    if (addr == 0)
        return nt_status::INTERNAL_ERROR;

    *base_address = (void*)addr;

    return nt_status::SUCCESS;
}

extern "C++"
NTSTATUS process::unmap_view_of_section(void* base_address) {
    auto m = find_mapping(*this, (uintptr_t)base_address);
    if (!m)
        return nt_status::NO_MEMORY;

    {
        lock_guard lock(mappings_lock);
        remove_entry_list(m->le);
    }

    m->release(); // because we're removing from the list
    m->release(); // and again because of find_mapping

    return nt_status::SUCCESS;
}

extern "C"
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

    if (!pptr->initialized) {
        print("process::process failed\n");
        delete pptr;
        return nt_status::INTERNAL_ERROR;
    }

    auto& p = *pptr;

    p.tree.key = next_pid.fetch_add(1);

    p.img = img;
    img.acquire();

#ifdef __aarch64__
    if (!p.allocate_ttbr0()) {
        print("process::allocate_ttbr0 failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }
#else
    if (!p.allocate_cr3()) {
        print("process::allocate_cr3 failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }
#endif

    p.img_addr = p.map_section(img, img.preferred_base + img.size > MAX_USERSPACE_ADDRESS ? 0 : img.preferred_base,
                               nullopt, mapping_type::read_only, true, nullptr);
    if (!p.img_addr) {
        print("process::map_section failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - if ntdll would have collision, choose another address

    p.ntdll_addr = p.map_section(*ntdll, ntdll->preferred_base,
                                 nullopt, mapping_type::read_only, true,
                                 nullptr);
    if (!p.ntdll_addr) {
        print("process::map_section failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - map time page into process

    if (!allocate_peb(p, 0, 0, 0, 0, "", string_view(img.name, img.namelen), u"")) {
        print("allocate_peb failed\n");
        p.release();
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - token for process

    // FIXME - give process a PID
    // FIXME - add process to list

    p.suspend_count = 0;

    {
        lock_guard lock(process_list_lock);
        insert_into_rbtree(process_tree, p.tree);
    }

    p.added_to_tree = true;

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

extern "C"
NTSTATUS NtDuplicateObject(HANDLE source_process_handle, HANDLE source_handle, HANDLE target_process_handle,
                           HANDLE* target_handle, ACCESS_MASK desired_access, uint32_t, uint32_t options) {
    object_ref srcprocobj, destprocobj;
    process* srcproc;
    process* destproc;
    ACCESS_MASK current_access;

    // FIXME - handle_attributes
    // FIXME - options (DUPLICATE_SAME_ATTRIBUTES, DUPLICATE_CLOSE_SOURCE)

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

    auto objrw = srcproc->deref_handle(source_handle, current_access);

    if (!objrw)
        return nt_status::INVALID_HANDLE;

    if (options & DUPLICATE_SAME_ACCESS)
        desired_access = current_access;
    else if (desired_access & ~current_access)
        return nt_status::ACCESS_DENIED;

    auto& obj = objrw.get();

    handle_t h;
    auto status = destproc->add_handle(obj, desired_access, true, h);

    if (!success(status))
        return status;

    if (target_handle)
        *target_handle = h;

    return nt_status::SUCCESS;
}

extern "C++"
NTSTATUS process::read_vm(void* base_address, void* buffer, uint32_t buffer_size,
                          uint32_t* number_of_bytes_read) {
    uint32_t copied = 0;

    while (true) {
        auto m = find_mapping(*this, (uintptr_t)base_address);
        if (!m)
            break;

        auto to_copy = min(buffer_size, (uint32_t)(m->address + m->size - (uintptr_t)base_address));

        if (!m->copy_from((uint32_t)((uintptr_t)base_address - m->address), buffer, (uint32_t)to_copy)) {
            m->release();
            return nt_status::INTERNAL_ERROR;
        }

        m->release();

        copied += to_copy;

        if (buffer_size == to_copy)
            break;

        base_address = (uint8_t*)base_address + to_copy;
        buffer = (uint8_t*)buffer + to_copy;
        buffer_size -= to_copy;
    }

    if (copied == 0)
        return nt_status::NO_MEMORY;

    if (number_of_bytes_read)
        *number_of_bytes_read = copied;

    return nt_status::SUCCESS;
}

extern "C"
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

extern "C++"
NTSTATUS process::write_vm(void* base_address, const void* buffer,
                           uint32_t buffer_size, uint32_t* number_of_bytes_written) {
    uint32_t copied = 0;

    while (true) {
        auto m = find_mapping(*this, (uintptr_t)base_address);
        if (!m)
            break;

        auto to_copy = min(buffer_size, (uint32_t)(m->address + m->size - (uintptr_t)base_address));

        if (!m->copy_to((uint32_t)((uintptr_t)base_address - m->address), buffer, (uint32_t)to_copy)) {
            m->release();
            return nt_status::INTERNAL_ERROR;
        }

        m->release();

        copied += to_copy;

        if (buffer_size == to_copy)
            break;

        base_address = (uint8_t*)base_address + to_copy;
        buffer = (uint8_t*)buffer + to_copy;
        buffer_size -= to_copy;
    }

    if (copied == 0)
        return nt_status::NO_MEMORY;

    if (number_of_bytes_written)
        *number_of_bytes_written = copied;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtWriteVirtualMemory(HANDLE process_handle, void* base_address, const void* buffer,
                              uint32_t buffer_size, uint32_t* number_of_bytes_written) {
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

extern "C++"
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
            pbi.UniqueProcessId = tree.key;
            pbi.InheritedFromUniqueProcessId = 0; // FIXME

            if (return_length)
                *return_length = sizeof(PROCESS_BASIC_INFORMATION);

            return nt_status::SUCCESS;
        }

        case PROCESSINFOCLASS::ImageFileName: {
            RTL_USER_PROCESS_PARAMETERS* params;
            UNICODE_STRING path_name;

            auto status = read_vm(&peb->ProcessParameters, &params, sizeof(params), nullptr);
            if (!success(status))
                return status;

            status = read_vm(&params->ImagePathName, &path_name, sizeof(path_name),
                             nullptr);
            if (!success(status))
                return status;

            auto len = (uint32_t)(sizeof(UNICODE_STRING) + path_name.Length + sizeof(char16_t));

            if (buf.size() < len) {
                if (return_length)
                    *return_length = len;

                return nt_status::INFO_LENGTH_MISMATCH;
            }

            auto& us = *(UNICODE_STRING*)buf.data();

            memset(&us, 0, sizeof(us));
            us.Length = path_name.Length;
            us.MaximumLength = us.Length + sizeof(char16_t);
            us.Buffer = (char16_t*)(buf.data() + sizeof(UNICODE_STRING));

            status = read_vm(path_name.Buffer, us.Buffer, us.Length, nullptr);
            if (!success(status))
                return status;

            us.Buffer[us.Length / sizeof(char16_t)] = 0;

            if (return_length)
                *return_length = len;

            return nt_status::SUCCESS;
        }

        default:
            print("process::query_info: unhandled info class {:x}\n",
                  (unsigned int)process_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

extern "C"
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

extern "C++"
object_ref process::get_stderr() {
    PEB peb;
    RTL_USER_PROCESS_PARAMETERS params;

    if (!copy_from(&peb, this->peb, sizeof(PEB)))
        return nullopt;

    if (!copy_from(&params, peb.ProcessParameters, sizeof(RTL_USER_PROCESS_PARAMETERS)))
        return nullopt;

    return this->deref_handle(params.StandardError);
}

extern "C++"
NTSTATUS process::create_thread(optional<reference_wrapper<handle_t>> thread_handle, ACCESS_MASK,
                                const OBJECT_ATTRIBUTES*, CLIENT_ID& client_id,
                                const CONTEXT& thread_context,
                                const INITIAL_TEB& initial_teb, bool create_suspended) {
    uint16_t tid;
    CONTEXT start_context;
    mn_image_info ntdll_info, img_info;

    {
        lock_guard lock(threads_lock);

        if (dying)
            return nt_status::PROCESS_IS_TERMINATING;
    }

    // FIXME - desired_access
    // FIXME - object_attributes

    auto tptr = new (align_val_t(16)) ::thread;
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
#ifndef __aarch64__
    start_context.EFlags |= 0x202; // IF, reserved flag
    start_context.SegCs = user_CS;
    start_context.SegSs = user_DS;
    start_context.SegEs = user_DS;
    start_context.SegFs = user_DS;
    start_context.SegGs = user_DS;
#endif

    if ((start_context.ContextFlags & CONTEXT_FLOATING_POINT) != CONTEXT_FLOATING_POINT) {
#ifdef __aarch64__
        memset(&start_context.V[0], 0, sizeof(start_context.V));
        start_context.Fpcr = 0;
        start_context.Fpsr = 0;
#else
        memset(&start_context.FltSave, 0, sizeof(start_context.FltSave));

        start_context.FltSave.MxCsr = 0x1f80;
        start_context.FltSave.MxCsr_Mask = 0x2ffff;
#endif

        start_context.ContextFlags |= CONTEXT_FLOATING_POINT;
    }

#ifndef __aarch64__
    start_context.MxCsr = start_context.FltSave.MxCsr;
#endif

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
    ntdll_info.offset_debug_line = ntdll->dwinfo.debug_line.offset;
    ntdll_info.size_debug_line = ntdll->dwinfo.debug_line.size;
    ntdll_info.offset_debug_line_str = ntdll->dwinfo.debug_line_str.offset;
    ntdll_info.size_debug_line_str = ntdll->dwinfo.debug_line_str.size;

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
    img_info.offset_debug_line = sect.dwinfo.debug_line.offset;
    img_info.size_debug_line = sect.dwinfo.debug_line.size;
    img_info.offset_debug_line_str = sect.dwinfo.debug_line_str.offset;
    img_info.size_debug_line_str = sect.dwinfo.debug_line_str.size;

    auto img_info_addr = ntdll_info_addr - sizeof(img_info);

    if (!copy_to((void*)img_info_addr, &img_info, sizeof(img_info))) {
        print("process::copy_to failed\n");
        delete &t;
        return nt_status::NO_MEMORY;
    }

    memset(&t.context, 0, sizeof(CONTEXT));

#ifdef __aarch64__
    t.context.Pc = ntdll_addr + mn_start_thread;
    t.context.X0 = ntdll_info_addr;
    t.context.X1 = img_info_addr;
    t.context.X2 = ctx_addr;
    t.context.X18 = (uintptr_t)t.teb;

    t.user_stack = (uintptr_t)img_info_addr - 0x10; // 0x10 for red zone
#else
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
#endif

    t.kernel_mode = false;

    t.suspend_count = create_suspended ? 1 : 0;
    t.status = thread_status::runnable;

    t.kernel_stack = allocate_stack();
    if (!t.kernel_stack) {
        delete &t;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    t.syscall_stack = (uint8_t*)t.kernel_stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT);

#ifdef __aarch64__
    t.syscall_stack = (uint8_t*)t.syscall_stack - 0x10; // 0x10 bytes for red zone
#else
    t.syscall_stack = (uint8_t*)t.syscall_stack - 0x20; // 0x20 bytes for MS ABI
#endif

    t.proc = this;
    this->acquire();

    bool proc_dying = false;

    {
        lock_guard lock(threads_lock);

        if (dying)
            proc_dying = true;
        else
            insert_tail_list(this->threads, t.process_le);
    }

    if (proc_dying) {
        delete &t;
        return nt_status::PROCESS_IS_TERMINATING;
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

extern "C++"
nt_status mapping::flush(uint64_t offset, uint64_t size) {
    return sect.flush((this->offset + offset) >> PAGE_SHIFT, size >> PAGE_SHIFT);
}

extern "C++"
nt_status process::flush_vm(void** base_address, size_t* region_size) {
    auto addr = (uint64_t)*base_address;

    addr &= ~(PAGE_SIZE - 1);

    auto m = find_mapping(*this, addr);
    if (!m)
        return nt_status::NO_MEMORY;

    auto size = *region_size;

    if (size & (PAGE_SIZE - 1))
        size = (size & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

    if (size == 0 || size > m->address + m->size - addr)
        size = m->address + m->size - addr;

    auto ret = m->flush(addr - m->address, size);
    if (!success(ret)) {
        m->release();
        return ret;
    }

    *base_address = (void*)addr;
    *region_size = size;

    m->release();

    return ret;
}

extern "C"
NTSTATUS NtFlushVirtualMemory(HANDLE proc_handle, void** base_address,
                              size_t* region_size, IO_STATUS_BLOCK* iosb) {
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

    memset(iosb, 0, sizeof(IO_STATUS_BLOCK)); // FIXME - what's supposed to go here?

    return p->flush_vm(base_address, region_size);
}

extern "C++"
void mapping::clear_write_bit(uint64_t page_num, uint64_t num_pages) {
#ifndef __aarch64__ // FIXME
    span<HARDWARE_PTE> pt[4];
    uint64_t va = address + (page_num << PAGE_SHIFT);
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    auto redo_level = [&](uint8_t level) {
        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    };

    for (uint8_t level = 3; level > 0; level--) {
        redo_level(level);
    }

    for (uint64_t i = 0; i < num_pages; i++) {
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

        if (pte.cow || pte.reserved)
            continue;

        pte.w = 0;
    }

    // send IPIs to invalidate TLBs on other processors

    auto cpus = proc.running_cpus.load();

    if (cpus == 0)
        return;

    {
        lock_guard lock(ipi_lock);

        disable_preempt();

        tlb_va = va;
        tlb_num_pages = num_pages;

        tlb_cpus_left = 1;

        for (uint8_t i = 0; i < num_cpus; i++) {
            if (!(cpus & 1 << i))
                continue;

            send_tlb_ipi(i);
        }

        tlb_cpus_left--;

        while (tlb_cpus_left > 0) { }

        enable_preempt();
    }
#endif
}

extern "C++"
nt_status process::get_next_thread(::thread* threadptr, ACCESS_MASK /*desired_access*/,
                                   uint32_t /*handle_attributes*/, uint32_t /*flags*/,
                                   HANDLE* new_thread_handle) {
    object_ref obj;
    handle_t h;

    // FIXME - desired_access, handle_attributes, flags
    // FIXME - skips threads that we don't have access to?

    {
        lock_guard lg(threads_lock);

        ::thread* next_thread;

        if (!threadptr) {
            if (is_list_empty(threads))
                return nt_status::NO_MORE_ENTRIES;

            next_thread = (::thread*)((uint8_t*)threads.next - offsetof(::thread, process_le));
        } else {
            if (threadptr->process_le.next == &threads)
                return nt_status::NO_MORE_ENTRIES;

            next_thread = (::thread*)((uint8_t*)threadptr->process_le.next - offsetof(::thread, process_le));
        }

        next_thread->acquire();
        obj = object_ref{*next_thread};
    }

    auto p = get_current_thread()->proc;

    auto status = p->add_handle(obj.get(), 0, true, h); // FIXME - access mask
    if (!success(status))
        return status;

    *new_thread_handle = h;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtGetNextThread(HANDLE process_handle, HANDLE thread_handle,
                         ACCESS_MASK desired_access, uint32_t handle_attributes,
                         uint32_t flags, HANDLE* new_thread_handle) {
    object_ref proc, threadobj;

    // FIXME - check access mask in process handle

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        auto p = t->proc;
        p->acquire();

        proc = object_ref{*p};
    } else {
        proc = deref_handle(process_handle);
        if (!proc)
            return nt_status::INVALID_HANDLE;

        auto& obj = proc.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;
    }

    ::thread* threadptr;

    if (thread_handle == NtCurrentThread()) {
        threadptr = get_current_thread();

        threadptr->acquire();

        threadobj = object_ref{*threadptr};
    } else if (thread_handle != 0) {
        threadobj = deref_handle(thread_handle);
        if (!threadobj)
            return nt_status::INVALID_HANDLE;

        auto& obj = threadobj.get();

        if (obj.type != object_type::thread)
            return nt_status::OBJECT_TYPE_MISMATCH;

        threadptr = static_cast<::thread*>(&obj);
    } else
        threadptr = nullptr;

    auto& p = static_cast<process&>(proc.get());

    if (threadobj) {
        auto& t = static_cast<::thread&>(threadobj.get());

        if (t.proc != &p)
            return nt_status::INVALID_PARAMETER;

        threadptr = &t;
    }

    return p.get_next_thread(threadptr, desired_access, handle_attributes,
                             flags, new_thread_handle);
}

extern "C"
NTSTATUS NtGetNextProcess(HANDLE process_handle, ACCESS_MASK /*desired_access*/,
                          ULONG /*handle_attributes*/, ULONG /*flags*/,
                          HANDLE* new_process_handle) {
    auto& t = *get_current_thread();
    object_ref old_proc;
    process* new_proc;

    // FIXME - desired_access, handle_attributes, flags

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        auto p = t->proc;
        p->acquire();

        old_proc = object_ref{*p};
    } else if (process_handle != 0) {
        old_proc = deref_handle(process_handle);
        if (!old_proc)
            return nt_status::INVALID_HANDLE;

        auto& obj = old_proc.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;
    }

    while (true) {
        {
            lock_guard lock(process_list_lock);

            if (!old_proc) {
                auto n = get_tree_minimum(process_tree);
                if (!n)
                    return nt_status::NO_MORE_ENTRIES;

                new_proc = &rbtree_containing_record<process>(n);
            } else {
                auto& p = static_cast<process&>(old_proc.get());

                auto n = next_rbtree_node(p.tree);
                if (!n)
                    return nt_status::NO_MORE_ENTRIES;

                new_proc = &rbtree_containing_record<process>(n);
            }

            new_proc->acquire();
        }

        if (new_proc->signalled) { // process is dead, go to next one
            old_proc = object_ref{*new_proc};
            continue;
        }

        break;
    }

    handle_t h;

    auto status = t.proc->add_handle(*new_proc, 0, true, h);
    if (!success(status)) {
        print("add_handle failed\n");
        new_proc->release();
        return status;
    }

    new_proc->release();

    *new_process_handle = h;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtOpenProcess(HANDLE* process_handle, ACCESS_MASK desired_access,
                       const OBJECT_ATTRIBUTES* /*object_attributes*/,
                       const CLIENT_ID* client_id) {
    auto& t = *get_current_thread();
    object_ref proc;

    if (!client_id)
        return nt_status::INVALID_PARAMETER;

    if (client_id->UniqueThread != 0 || client_id->UniqueProcess == 0)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(process_list_lock);

        auto n = find_rbtree_node(process_tree, client_id->UniqueProcess);

        if (!n || n->key != client_id->UniqueProcess)
            return nt_status::INVALID_CID;

        auto& p = rbtree_containing_record<process>(n);

        p.acquire();
        proc = p;
    }

    handle_t h;

    auto status = t.proc->add_handle(proc.get(), desired_access, true, h);
    if (!success(status)) {
        print("add_handle failed\n");
        static_cast<process&>(proc.get()).release();
        return status;
    }

    *process_handle = h;

    return nt_status::SUCCESS;
}
