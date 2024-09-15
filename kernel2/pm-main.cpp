#include <stdbool.h>
#include <stdint.h>
#include <intrin.h>
#include "x86.h"
#include "pe.h"

using namespace std;

extern e820_entry* ptr_e820_entries;
extern uint32_t* ptr_e820_count;
extern uint8_t* payload_data;
extern const uint64_t payload_size;

static e820_entry* mem_entries;
static uint32_t mem_entries_count;

// FIXME - move to another file?
extern "C"
void* memset(void* s, int c, size_t n) {
    void* orig_s = s;

    // FIXME - faster if we make sure we're aligned (also in memcpy)?

#if __INTPTR_WIDTH__ == 64
    uint64_t v;

    v = 0;

    for (unsigned int i = 0; i < sizeof(uint64_t); i++) {
        v <<= 8;
        v |= c & 0xff;
    }

    while (n >= sizeof(uint64_t)) {
        *(uint64_t*)s = v;

        s = (uint8_t*)s + sizeof(uint64_t);
        n -= sizeof(uint64_t);
    }
#else
    uint32_t v;

    v = 0;

    for (unsigned int i = 0; i < sizeof(uint32_t); i++) {
        v <<= 8;
        v |= c & 0xff;
    }

    while (n >= sizeof(uint32_t)) {
        *(uint32_t*)s = v;

        s = (uint8_t*)s + sizeof(uint32_t);
        n -= sizeof(uint32_t);
    }
#endif

    while (n > 0) {
        *(uint8_t*)s = c;

        s = (uint8_t*)s + 1;
        n--;
    }

    return orig_s;
}

static void add_new_mem_entry(unsigned int pos) {
    for (int i = mem_entries_count; i >= pos; i--) {
        mem_entries[i + 1] = mem_entries[i];
    }
    mem_entries_count++;
}

static void remove_mem_entry(unsigned int pos) {
    for (unsigned int i = pos; i < mem_entries_count; i++) {
        mem_entries[i] = mem_entries[i + 1];
    }
    mem_entries_count--;
}

static uint64_t get_page(enum mem_type type = mem_type::bootloader) {
    uint64_t ret = 0;

    // FIXME - make sure in first 4GB
    // FIXME - handle allocation failure elsewhere

    for (unsigned int i = 0; i < mem_entries_count; i++) {
        auto& m = mem_entries[i];

        if (m.type != mem_type::free)
            continue;

        ret = m.address;

        // extending previous entry
        if (i > 0 && mem_entries[i - 1].type == type) {
            mem_entries[i - 1].length += PAGE_SIZE;
            m.address += PAGE_SIZE;
            m.length -= PAGE_SIZE;

            if (m.length == 0)
                remove_mem_entry(i);

            break;
        }

        // free block of only one page
        if (m.length == PAGE_SIZE) {
            m.type = type;
            break;
        }

        // add new entry
        add_new_mem_entry(i + 1);

        mem_entries[i + 1].address = ret + PAGE_SIZE;
        mem_entries[i + 1].length = m.length - PAGE_SIZE;
        mem_entries[i + 1].type = mem_type::free;

        m.length = PAGE_SIZE;
        m.type = type;

        break;
    }

    return ret;
}

static void map_page(pte_span pt3, uint64_t phys, uint64_t virt) {
    auto& pt3_ent = pt3[(virt >> 39) & 0x1ff];

    if (!pt3_ent.Valid) {
        pt3_ent.Valid = 1;
        pt3_ent.PageFrameNumber = get_page(mem_type::page_table) >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt3_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt2((HARDWARE_PTE_PAE*)(uintptr_t)(pt3_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto& pt2_ent = pt2[(virt >> 30) & 0x1ff];

    if (!pt2_ent.Valid) {
        pt2_ent.Valid = 1;
        pt2_ent.Write = 1;
        pt2_ent.PageFrameNumber = get_page(mem_type::page_table) >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt2_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt1((HARDWARE_PTE_PAE*)(uintptr_t)(pt2_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto &pt1_ent = pt1[(virt >> 21) & 0x1ff];

    if (!pt1_ent.Valid) {
        pt1_ent.Valid = 1;
        pt1_ent.Write = 1;
        pt1_ent.PageFrameNumber = get_page(mem_type::page_table) >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt1_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt0((HARDWARE_PTE_PAE*)(uintptr_t)(pt1_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto &pt0_ent = pt0[(virt >> 12) & 0x1ff];

    pt0_ent.Valid = 1;
    pt0_ent.Write = 1;
    pt0_ent.PageFrameNumber = phys >> PAGE_SHIFT;
}

static void enable_paging() {
    pte_span pd((HARDWARE_PTE_PAE*)(uintptr_t)get_page(mem_type::page_table), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    memset(pd.data(), 0, pd.size_bytes());

    span mem(mem_entries, mem_entries_count);

    for (unsigned int i = 0; i < mem_entries_count; i++) {
        auto& m = mem_entries[i];
        if (m.type != mem_type::bootloader)
            continue;

        for (uint64_t j = 0; j < m.length; j += PAGE_SIZE) {
            map_page(pd, m.address + j, m.address + j);
        }
    }

    // enable PAE
    __writecr4(__readcr4() | CR4_PAE);

    // set cr3
    __writecr3((uintptr_t)pd.data());

    __writemsr(EFER_MSR, __readmsr(EFER_MSR) | EFER_LME);

    // enable paging
    __writecr0(__readcr0() | CR0_PG);
}

void double_fault() { // FIXME
    while (true) { }
}

static consteval gdt_entry make_gdt_entry(uint32_t base, uint32_t limit, uint8_t type,
                                          uint8_t dpl, bool pres, bool sys, bool long_mode,
                                          bool default_big, bool granularity) {
    gdt_entry g;

    g.LimitLow = limit & 0xffff;
    g.LimitHi = (limit >> 16) & 0xf;

    g.BaseLow = base & 0xffff;
    g.BaseMid = (base >> 16) & 0xff;
    g.BaseHi = (base >> 24) & 0xff;

    g.Type = type;
    g.Dpl = dpl;
    g.Pres = pres ? 1 : 0;

    g.Sys = sys;
    g.Long = long_mode;
    g.Default_Big = default_big;
    g.Granularity = granularity;

    return g;
}

static void setup_gdt() {
    GDTIDT desc;

    auto page = get_page();

    memset((void*)(uintptr_t)page, 0, PAGE_SIZE);

    span gdt((gdt_entry*)(uintptr_t)page, 4);

    gdt[0] = make_gdt_entry(0, 0, 0, 0, false, false, false, false, false); // null descriptor
    gdt[1] = make_gdt_entry(0, 0xfffff, 0x1a, 0, true, false, false, true, true); // kernel-mode 32-bit code
    gdt[2] = make_gdt_entry(0, 0xfffff, 0x12, 0, true, false, false, true, true); // kernel-mode data
    gdt[3] = make_gdt_entry(0, 0xfffff, 0x1a, 0, true, false, true, false, true); // kernel-mode 64-bit code

    desc.Base = (uintptr_t)page;
    desc.Limit = gdt.size_bytes() - 1;

    __asm__ __volatile__ (
        "lgdt %0\n\t"
        "push 0x8\n\t"
        "push OFFSET 1f\n\t"
        "retf\n\t"
        "1:\n\t"
        :
        : "m" (desc)
    );
}

static void setup_idt_entry(idt_entry& i, void* isr, uint16_t access) {
    i.offset_1 = (uintptr_t)isr & 0xffff;
    i.selector = 0x8;
    i.type_attributes = access;
    i.offset_2 = (uintptr_t)isr >> 16;
}

static void setup_idt() {
    GDTIDT desc;

    auto page = get_page();

    memset((void*)(uintptr_t)page, 0, PAGE_SIZE);

    span idt((idt_entry*)(uintptr_t)page, NUM_IDT);

    setup_idt_entry(idt[8], (void*)double_fault, 0x8f);

    desc.Base = (uintptr_t)page;
    desc.Limit = (NUM_IDT * sizeof(idt_entry)) - 1;

    __asm__ __volatile__ (
        "lidt %0\n\t"
        :
        : "m" (desc)
    );
}

weak_ordering cmp_mem_type(enum mem_type m1, enum mem_type m2) {
    if ((unsigned int)m1 == (unsigned int)m2)
        return weak_ordering::equivalent;

    if (m1 == mem_type::free)
        return weak_ordering::less;

    if (m2 == mem_type::free)
        return weak_ordering::greater;

    if (m1 == mem_type::reserved)
        return weak_ordering::greater;

    if (m2 == mem_type::reserved)
        return weak_ordering::less;

    if (m1 == mem_type::bad)
        return weak_ordering::greater;

    if (m2 == mem_type::bad)
        return weak_ordering::less;

    return weak_ordering::equivalent;
}

static void sanitize_mem_entries() {
    span mem(mem_entries, mem_entries_count);

    // align to pages
    for (auto& m : mem) {
        if (m.address & (PAGE_SIZE - 1)) {
            uint64_t new_addr = m.address & ~(PAGE_SIZE - 1);

            m.length += m.address - new_addr;
            m.address = new_addr;
        }

        if (m.length & (PAGE_SIZE - 1))
            m.length = (m.length & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
    }

    // remove any with size 0, and change unknown types to mem_type::reserved
    for (unsigned int i = 0; i < mem_entries_count; i++) {
        if (mem_entries[i].length == 0) {
            swap(mem_entries[i], mem_entries[mem_entries_count - 1]);
            mem_entries_count--;
        } else if (mem_entries[i].type > mem_type::max_bios)
            mem_entries[i].type = mem_type::reserved;
    }

    // sort
    while (true) {
        bool changed = false;

        for (unsigned i = 0; i < mem_entries_count - 1; i++) {
            auto& a = mem_entries[i];
            auto& b = mem_entries[i + 1];

            if (a.address > b.address) {
                swap(a, b);
                changed = true;
                i--;
            }
        }

        if (!changed)
            break;
    }

    // handle overlaps
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (b.address >= a.address && b.address < a.address + a.length) {
            auto cmp = cmp_mem_type(a.type, b.type);

            if (b.address + b.length < a.address + a.length) {
                // second entirely within first

                if (cmp == weak_ordering::less) {
                    uint64_t a_end = a.address + a.length;

                    a.length = b.address - a.address;

                    // FIXME - check for overflow
                    add_new_mem_entry(i + 2);

                    auto& c = mem_entries[i + 2];
                    c.address = b.address + b.length;
                    c.length = a_end - c.address;
                    c.type = a.type;
                } else
                    b.length = 0;
            } else {
                if (cmp == weak_ordering::less)
                    a.length = b.address - a.address;
                else {
                    b.length -= a.address + a.length - b.address;
                    b.address = a.address + a.length;
                }
            }

            auto new_i = i;

            if (b.length == 0) {
                remove_mem_entry(i + 1);
                new_i--;
            }

            if (a.length == 0)
                remove_mem_entry(i);

            i = new_i;
        }
    }

    // add any missing
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (b.address > a.address + a.length) {
            uint64_t new_address = a.address + a.length;
            uint64_t new_length = b.address - a.address - a.length;

            add_new_mem_entry(i + 1);
            mem_entries[i + 1].address = new_address;
            mem_entries[i + 1].length = new_length;
            mem_entries[i + 1].type = mem_type::reserved;

            i--;
        }
    }

    // combine adjacent
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (a.type == b.type) {
            a.length = b.address + b.length - a.address;
            remove_mem_entry(i + 1);
            i--;
        }
    }

    // remove bad or reserved bits at end
    while (mem_entries_count > 0 && (mem_entries[mem_entries_count - 1].type == mem_type::bad ||
         mem_entries[mem_entries_count - 1].type == mem_type::reserved)) {
        mem_entries_count--;
    }
}

static void mark_bootloader_memory() {
    uint64_t bootloader_end;

    bootloader_end = (uintptr_t)&payload_data + payload_size;

    if (bootloader_end & (PAGE_SIZE - 1))
        bootloader_end = (bootloader_end & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

    span mem(mem_entries, mem_entries_count);

    for (unsigned int i = 0; i < mem_entries_count; i++) {
        auto& m = mem_entries[i];

        if (m.type != mem_type::free)
            continue;

        if (m.address >= bootloader_end)
            break;

        if (m.address + m.length <= bootloader_end) {
            m.type = mem_type::bootloader;
            continue;
        }

        add_new_mem_entry(i + 1);

        auto& m2 = mem_entries[i + 1];
        m2.address = bootloader_end;
        m2.length = m.address + m.length - bootloader_end;
        m2.type = m.type;

        m.length = bootloader_end - m.address;
        m.type = mem_type::bootloader;
        break;
    }
}

static bool load_payload() {
    auto& dos_header = *(IMAGE_DOS_HEADER*)&payload_data;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    auto& pe_header = *(IMAGE_NT_HEADERS*)((uint8_t*)&dos_header + dos_header.e_lfanew);

    if (pe_header.Signature != IMAGE_NT_SIGNATURE)
        return false;

    if (pe_header.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
        return false;

    if (pe_header.OptionalHeader64.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;

    // FIXME - reserve contiguous space for PE image
    // FIXME - copy sections into space, padding with zeroes if necessary
    // FIXME - do relocations

    return true;
}

extern "C"
void protected_mode_main() {
    mem_entries = ptr_e820_entries;
    mem_entries_count = *ptr_e820_count;

    sanitize_mem_entries();
    mark_bootloader_memory();

    setup_gdt();
    setup_idt();

    if (!load_payload()) {
        // FIXME - display error message
        while (true) { }
    }

    enable_paging();

    // FIXME - rm 32-bit IDT

    // FIXME - jump to entry point of image

    __asm__ __volatile__ (
        "push 0x18\n\t"
        "push OFFSET long_mode_start\n\t"
        "retf\n\t"
    );
}
