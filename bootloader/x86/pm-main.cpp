#include <stdbool.h>
#include <stdint.h>
#include <string_view>
#include "x86.h"

import pe;
import loaderblock;

using namespace std;

struct vbe_mode_info {
    uint16_t mode_number;
    uint16_t attributes;
    uint8_t window_a;
    uint8_t window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t w_char;
    uint8_t y_char;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved0;
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;
    uint32_t framebuffer;
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;
} __attribute__ ((packed));

static_assert(sizeof(vbe_mode_info) == 52);

static const uint8_t MM_TEXT = 0;

extern e820_entry* ptr_e820_entries;
extern uint32_t* ptr_e820_count;
extern uint8_t payload_data;
extern const uint64_t payload_size;
extern char* ptr_cmdline;
extern uint8_t* ptr_initrd_data;
extern uint32_t* ptr_initrd_size;
extern vbe_mode_info* ptr_vbe_modes;
extern uint32_t pm_vbe_selected_mode;
extern void* ptr_vbe_edid;
extern bool* ptr_vbe_edid_valid;

// pm.s
extern "C" bool have_long_mode();

static const unsigned int MAX_MAP_ENTRIES = 128; // same as E820_COUNT

static map_entry mem_entries[MAX_MAP_ENTRIES];
static uint32_t mem_entries_count;
static uint64_t debug_info_ptr, debug_abbrev_ptr, debug_rnglists_ptr,
                debug_str_ptr, debug_addr_ptr, debug_str_offsets_ptr,
                debug_line_ptr, debug_line_str_ptr;
static uint32_t debug_info_size, debug_abbrev_size, debug_rnglists_size,
                debug_str_size, debug_addr_size, debug_str_offsets_size,
                debug_line_size, debug_line_str_size;

static const uint64_t img_addr = 0xfffff80000000000;

static uintptr_t initrd_addr = 0;

static span<uint16_t> text_console;
enum class console_colour : uint8_t {
    black = 0,
    blue = 1,
    green = 2,
    cyan = 3,
    red = 4,
    magenta = 5,
    brown = 6,
    light_grey = 7,
    dark_grey = 8,
    light_blue = 9,
    light_green = 10,
    light_cyan = 11,
    light_red = 12,
    light_magenta = 13,
    yellow = 14,
    white = 15
};

static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static const enum console_colour fg_colour = console_colour::light_grey;
static const enum console_colour bg_colour = console_colour::black;

static const uint64_t RSD_PTR = 0x2052545020445352; // "RSD PTR "
static const uint32_t RSDT = 0x54445352; // "RSDT"
static const uint32_t XSDT = 0x54445358; // "XSDT"

struct RSDPDescriptor {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__((packed));

struct XSDPDescriptor {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} __attribute__ ((packed));

struct ACPISDTHeader {
    uint32_t Signature;
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__ ((packed));

uint64_t entry_point;
static uint64_t rsdp_pa = 0, rsdt_address = 0;
loader_block lb;

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

extern "C"
void* memcpy(void* dest, const void* src, size_t n) {
    void* orig_dest = dest;

#if __INTPTR_WIDTH__ == 64
    while (n >= sizeof(uint64_t)) {
        *(uint64_t*)dest = *(uint64_t*)src;

        dest = (uint8_t*)dest + sizeof(uint64_t);
        src = (uint8_t*)src + sizeof(uint64_t);

        n -= sizeof(uint64_t);
    }
#endif

    while (n >= sizeof(uint32_t)) {
        *(uint32_t*)dest = *(uint32_t*)src;

        dest = (uint8_t*)dest + sizeof(uint32_t);
        src = (uint8_t*)src + sizeof(uint32_t);

        n -= sizeof(uint32_t);
    }

    while (n >= sizeof(uint16_t)) {
        *(uint16_t*)dest = *(uint16_t*)src;

        dest = (uint8_t*)dest + sizeof(uint16_t);
        src = (uint8_t*)src + sizeof(uint16_t);

        n -= sizeof(uint16_t);
    }

    while (n >= sizeof(uint8_t)) {
        *(uint8_t*)dest = *(uint8_t*)src;

        dest = (uint8_t*)dest + sizeof(uint8_t);
        src = (uint8_t*)src + sizeof(uint8_t);

        n -= sizeof(uint8_t);
    }

    return orig_dest;
}

extern "C"
size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

extern "C"
int memcmp(const void* s1, const void* s2, size_t n) {
    // FIXME - not completely accurate, as 1 or -1 is messed up by endianness

    while (n > sizeof(uint64_t)) {
        uint64_t c1 = *(uint64_t*)s1;
        uint64_t c2 = *(uint64_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint64_t*)s1 + 1;
        s2 = (uint64_t*)s2 + 1;
        n -= sizeof(uint64_t);
    }

    while (n > sizeof(uint32_t)) {
        uint32_t c1 = *(uint32_t*)s1;
        uint32_t c2 = *(uint32_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint32_t*)s1 + 1;
        s2 = (uint32_t*)s2 + 1;
        n -= sizeof(uint32_t);
    }

    while (n > sizeof(uint16_t)) {
        uint16_t c1 = *(uint16_t*)s1;
        uint16_t c2 = *(uint16_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint16_t*)s1 + 1;
        s2 = (uint16_t*)s2 + 1;
        n -= sizeof(uint16_t);
    }

    while (n > 0) {
        uint8_t c1 = *(uint8_t*)s1;
        uint8_t c2 = *(uint8_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint8_t*)s1 + 1;
        s2 = (uint8_t*)s2 + 1;
        n--;
    }

    return 0;
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

static uint64_t get_pages(enum mem_type type, unsigned int num_pages) {
    uint64_t ret = 0;

    // FIXME - make sure in first 4GB
    // FIXME - handle allocation failure elsewhere

    for (unsigned int i = 0; i < mem_entries_count; i++) {
        auto& m = mem_entries[i];

        if (m.type != mem_type::free || m.len < num_pages * PAGE_SIZE)
            continue;

        ret = m.addr;

        // extending previous entry
        if (i > 0 && mem_entries[i - 1].type == type) {
            mem_entries[i - 1].len += num_pages * PAGE_SIZE;
            m.addr += num_pages * PAGE_SIZE;
            m.len -= num_pages * PAGE_SIZE;

            if (m.len == 0)
                remove_mem_entry(i);

            break;
        }

        // free block of same size
        if (m.len == num_pages * PAGE_SIZE) {
            m.type = type;
            break;
        }

        // add new entry
        add_new_mem_entry(i + 1);

        mem_entries[i + 1].addr = ret + (num_pages * PAGE_SIZE);
        mem_entries[i + 1].len = m.len - (num_pages * PAGE_SIZE);
        mem_entries[i + 1].type = mem_type::free;

        m.len = num_pages * PAGE_SIZE;
        m.type = type;

        break;
    }

    return ret;
}

static uint64_t get_page(enum mem_type type = mem_type::bootloader) {
    return get_pages(type, 1);
}

static void map_page(pte_span pt3, uint64_t phys, uint64_t virt) {
    auto& pt3_ent = pt3[(virt >> 39) & 0x1ff];

    if (!pt3_ent.Valid) {
        pt3_ent.Valid = 1;
        pt3_ent.Write = 1;
        pt3_ent.PageFrameNumber = get_page(mem_type::paging_table) >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt3_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt2((HARDWARE_PTE_PAE*)(uintptr_t)(pt3_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto& pt2_ent = pt2[(virt >> 30) & 0x1ff];

    if (!pt2_ent.Valid) {
        pt2_ent.Valid = 1;
        pt2_ent.Write = 1;
        pt2_ent.PageFrameNumber = get_page(mem_type::paging_table) >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt2_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt1((HARDWARE_PTE_PAE*)(uintptr_t)(pt2_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto& pt1_ent = pt1[(virt >> 21) & 0x1ff];

    if (!pt1_ent.Valid) {
        pt1_ent.Valid = 1;
        pt1_ent.Write = 1;
        pt1_ent.PageFrameNumber = get_page(mem_type::paging_table) >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt1_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt0((HARDWARE_PTE_PAE*)(uintptr_t)(pt1_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto &pt0_ent = pt0[(virt >> 12) & 0x1ff];

    pt0_ent.Valid = 1;
    pt0_ent.Write = 1;
    pt0_ent.PageFrameNumber = phys >> PAGE_SHIFT;
}

static void map_acpi_table(pte_span pd, const ACPISDTHeader& rsdt) {
    uint64_t start_page = rsdt_address & ~(PAGE_SHIFT - 1);
    uint64_t end_page = (rsdt_address + rsdt.Length - 1) & ~(PAGE_SHIFT - 1);

    for (uint64_t i = start_page; i <= end_page; i++) {
        map_page(pd, i, i);
    }
}

static void map_acpi(pte_span pd) {
    const auto& rsdt = *(ACPISDTHeader*)rsdt_address;

    map_acpi_table(pd, rsdt);

    switch (rsdt.Signature) {
        case RSDT: {
            span entries((uint32_t*)((uint8_t*)&rsdt + sizeof(ACPISDTHeader)), (rsdt.Length - sizeof(ACPISDTHeader)) / sizeof(uint32_t));

            for (auto e : entries) {
                map_acpi_table(pd, *(ACPISDTHeader*)e);
            }

            break;
        }

        case XSDT: {
            span entries((uint64_t*)((uint8_t*)&rsdt + sizeof(ACPISDTHeader)), (rsdt.Length - sizeof(ACPISDTHeader)) / sizeof(uint64_t));

            for (auto e : entries) {
                map_acpi_table(pd, *(ACPISDTHeader*)e);
            }

            break;
        }
    }
}

__attribute__((always_inline))
static uint32_t inline __readcr0() {
    uint32_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr0"
        : "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
static void inline __writecr0(uint32_t v) {
    __asm__ __volatile__ (
        "mov\tcr0, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
static void inline __writecr3(uint32_t v) {
    __asm__ __volatile__ (
        "mov\tcr3, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
static uint32_t inline __readcr4() {
    uint32_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr4"
        : "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
static void inline __writecr4(uint32_t v) {
    __asm__ __volatile__ (
        "mov\tcr4, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
static uint64_t inline __readmsr(uint32_t r) {
    uint32_t high, low;

    __asm__ __volatile__ (
        "rdmsr"
        : "=d" (high), "=a" (low)
        : "c" (r)
    );

    return ((uint64_t)high << 32) | low;
}

__attribute__((always_inline))
static void inline __writemsr(uint32_t r, uint64_t v) {
    uint32_t high = v >> 32;
    uint32_t low = v & 0xffffffff;

    __asm__ __volatile__ (
        "wrmsr"
        :
        : "c" (r), "d" (high), "a" (low)
    );
}

static void enable_paging() {
    pte_span pd((HARDWARE_PTE_PAE*)(uintptr_t)get_page(mem_type::paging_table), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    memset(pd.data(), 0, pd.size_bytes());

    span mem(mem_entries, mem_entries_count);

    for (unsigned int i = 0; i < mem_entries_count; i++) {
        auto& m = mem_entries[i];

        switch (m.type) {
            case mem_type::bootloader:
            case mem_type::initrd:
                for (uint64_t j = 0; j < m.len; j += PAGE_SIZE) {
                    map_page(pd, m.addr + j, m.addr + j);
                }
            break;

            case mem_type::image:
                for (uint64_t j = 0; j < m.len; j += PAGE_SIZE) {
                    map_page(pd, m.addr + j, img_addr + j);
                }
            break;

            default:
            break;
        }
    }

    if (rsdt_address != 0)
        map_acpi(pd);

    pd[(SELFMAP & 0xff8000000000) >> 39].PageFrameNumber = (uintptr_t)pd.data() >> PAGE_SHIFT;
    pd[(SELFMAP & 0xff8000000000) >> 39].Valid = 1;
    pd[(SELFMAP & 0xff8000000000) >> 39].Write = 1;

    // enable PAE, and set OXFSXR and OSXMMEXCPT bits on cr4
    __writecr4(__readcr4() | CR4_PAE | CR4_OSFXSR | CR4_OSXMMEXCPT);

    // set cr3
    __writecr3((uintptr_t)pd.data());

    // enable long mode, NX bit, and syscalls
    __writemsr(EFER_MSR, __readmsr(EFER_MSR) | EFER_LME | EFER_NX | EFER_SCE);

    // enable paging and set MP and WP bits
    __writecr0(__readcr0() | CR0_PG | CR0_MP | CR0_WP);
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
        "lea eax, 1f\n\t"
        "push eax\n\t"
        "retf\n\t"
        "1:\n\t"
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

    return weak_ordering::equivalent;
}

static void sanitize_mem_entries() {
    span mem(mem_entries, mem_entries_count);

    // align to pages
    for (auto& m : mem) {
        if (m.addr & (PAGE_SIZE - 1)) {
            uint64_t new_addr = m.addr & ~(PAGE_SIZE - 1);

            m.len += m.addr - new_addr;
            m.addr = new_addr;
        }

        if (m.len & (PAGE_SIZE - 1))
            m.len = (m.len & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
    }

    // remove any with size 0
    for (unsigned int i = 0; i < mem_entries_count; i++) {
        if (mem_entries[i].len == 0) {
            swap(mem_entries[i], mem_entries[mem_entries_count - 1]);
            mem_entries_count--;
        }
    }

    // sort
    while (true) {
        bool changed = false;

        for (unsigned i = 0; i < mem_entries_count - 1; i++) {
            auto& a = mem_entries[i];
            auto& b = mem_entries[i + 1];

            if (a.addr > b.addr) {
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

        if (b.addr >= a.addr && b.addr < a.addr + a.len) {
            auto cmp = cmp_mem_type(a.type, b.type);

            if (b.addr + b.len < a.addr + a.len) {
                // second entirely within first

                if (cmp == weak_ordering::less) {
                    uint64_t a_end = a.addr + a.len;

                    a.len = b.addr - a.addr;

                    // FIXME - check for overflow
                    add_new_mem_entry(i + 2);

                    auto& c = mem_entries[i + 2];
                    c.addr = b.addr + b.len;
                    c.len = a_end - c.addr;
                    c.type = a.type;
                } else
                    b.len = 0;
            } else {
                if (cmp == weak_ordering::less)
                    a.len = b.addr - a.addr;
                else {
                    b.len -= a.addr + a.len - b.addr;
                    b.addr = a.addr + a.len;
                }
            }

            auto new_i = i;

            if (b.len == 0) {
                remove_mem_entry(i + 1);
                new_i--;
            }

            if (a.len == 0)
                remove_mem_entry(i);

            i = new_i;
        }
    }

    // add any missing
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (b.addr > a.addr + a.len) {
            uint64_t new_address = a.addr + a.len;
            uint64_t new_length = b.addr - a.addr - a.len;

            add_new_mem_entry(i + 1);
            mem_entries[i + 1].addr = new_address;
            mem_entries[i + 1].len = new_length;
            mem_entries[i + 1].type = mem_type::reserved;

            i--;
        }
    }

    // combine adjacent
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (a.type == b.type) {
            a.len = b.addr + b.len - a.addr;
            remove_mem_entry(i + 1);
            i--;
        }
    }

    // remove bad or reserved bits at end
    while (mem_entries_count > 0 && mem_entries[mem_entries_count - 1].type == mem_type::reserved) {
        mem_entries_count--;
    }
}

static void mark_bootloader_memory() {
    uint64_t bootloader_end;

    bootloader_end = (uintptr_t)&payload_data + payload_size;

    if (bootloader_end & (PAGE_SIZE - 1))
        bootloader_end = (bootloader_end & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

    for (unsigned int i = 0; i < mem_entries_count; i++) {
        auto& m = mem_entries[i];

        if (m.type != mem_type::free)
            continue;

        if (m.addr >= bootloader_end)
            break;

        if (m.addr + m.len <= bootloader_end) {
            m.type = mem_type::bootloader;
            continue;
        }

        add_new_mem_entry(i + 1);

        auto& m2 = mem_entries[i + 1];
        m2.addr = bootloader_end;
        m2.len = m.addr + m.len - bootloader_end;
        m2.type = m.type;

        m.len = bootloader_end - m.addr;
        m.type = mem_type::bootloader;
        break;
    }
}

static bool do_relocations(IMAGE_NT_HEADERS& pe_header, uint8_t* data, uint64_t va) {
    if (pe_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
        pe_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
        pe_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
        return true;
    }

    auto size = pe_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

    auto reloc = (IMAGE_BASE_RELOCATION*)(data + pe_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

    auto base = pe_header.OptionalHeader64.ImageBase;

    do {
        if (size < reloc->SizeOfBlock)
            return true;

        // FIXME - check not out of bounds
        auto ptr = (uint32_t*)(data + reloc->VirtualAddress);

        auto addr = (uint16_t*)((uint8_t*)reloc + sizeof(IMAGE_BASE_RELOCATION));
        auto count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);

        for (unsigned int i = 0; i < count; i++) {
            uint16_t offset = addr[i] & 0xfff;
            uint16_t type = addr[i] >> 12;

            switch (type) {
                case IMAGE_REL_BASED_ABSOLUTE:
                    // nop
                break;

                case IMAGE_REL_BASED_HIGHLOW:
                {
                    uint32_t* ptr2 = (uint32_t*)((uint8_t*)ptr + offset);

                    *ptr2 = *ptr2 - base + va;
                    break;
                }

                case IMAGE_REL_BASED_DIR64:
                {
                    uint64_t* ptr2 = (uint64_t*)((uint8_t*)ptr + offset);

                    *ptr2 = *ptr2 - base + va;
                    break;
                }

                default: {
                    // FIXME - show error for unsupported relocation type
                    return false;
                }
            }
        }

        size -= reloc->SizeOfBlock;

        if (size < sizeof(IMAGE_BASE_RELOCATION))
            break;

        reloc = (IMAGE_BASE_RELOCATION*)((uint8_t*)reloc + reloc->SizeOfBlock);
    } while (true);

    return true;
}

static string_view get_section_name(const IMAGE_SECTION_HEADER& s, const IMAGE_FILE_HEADER& file_header,
                                    span<const uint8_t> payload) {
    auto name = string_view(s.Name, sizeof(s.Name));

    while (!name.empty() && name.back() == 0) {
        name.remove_suffix(1);
    }

    if (name.empty() || name[0] != '/' || file_header.PointerToSymbolTable == 0)
        return name;

    // decode long section names

    unsigned int num = 0;
    uint32_t string_table = file_header.PointerToSymbolTable + (file_header.NumberOfSymbols * 18);

    for (auto c : string_view(name.data() + 1, name.size() - 1)) {
        num *= 10;

        if (c >= '0' && c <= '9')
            num += c - '0';
        else
            return name;
    }

    return string_view((char*)payload.data() + string_table + num);
}

static void print(string_view s);

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

    auto img_size = pe_header.OptionalHeader64.SizeOfImage;

    if (img_size & (PAGE_SIZE - 1))
        img_size = (img_size & ~(PAGE_SIZE - 1)) + PAGE_SIZE;

    auto addr = (uint8_t*)get_pages(mem_type::image, img_size / PAGE_SIZE);

    // copy header

    memcpy(addr, &dos_header, pe_header.OptionalHeader64.SizeOfHeaders);

    if (pe_header.OptionalHeader64.SizeOfHeaders < PAGE_SIZE) {
        memset(addr + pe_header.OptionalHeader64.SizeOfHeaders, 0,
               PAGE_SIZE - pe_header.OptionalHeader64.SizeOfHeaders);
    }

    // copy sections

    span sects((IMAGE_SECTION_HEADER*)((uint8_t*)&pe_header.OptionalHeader64 + pe_header.FileHeader.SizeOfOptionalHeader),
               pe_header.FileHeader.NumberOfSections);

    for (const auto& s : sects) {
        uint32_t section_size;
        auto name = get_section_name(s, pe_header.FileHeader,
                                     span(&payload_data, payload_size));

        if (name == ".debug_info") {
            debug_info_ptr = img_addr + s.VirtualAddress;
            debug_info_size = s.VirtualSize;
        } else if (name == ".debug_abbrev") {
            debug_abbrev_ptr = img_addr + s.VirtualAddress;
            debug_abbrev_size = s.VirtualSize;
        } else if (name == ".debug_rnglists") {
            debug_rnglists_ptr = img_addr + s.VirtualAddress;
            debug_rnglists_size = s.VirtualSize;
        } else if (name == ".debug_str") {
            debug_str_ptr = img_addr + s.VirtualAddress;
            debug_str_size = s.VirtualSize;
        } else if (name == ".debug_addr") {
            debug_addr_ptr = img_addr + s.VirtualAddress;
            debug_addr_size = s.VirtualSize;
        } else if (name == ".debug_str_offsets") {
            debug_str_offsets_ptr = img_addr + s.VirtualAddress;
            debug_str_offsets_size = s.VirtualSize;
        } else if (name == ".debug_line") {
            debug_line_ptr = img_addr + s.VirtualAddress;
            debug_line_size = s.VirtualSize;
        } else if (name == ".debug_line_str") {
            debug_line_str_ptr = img_addr + s.VirtualAddress;
            debug_line_str_size = s.VirtualSize;
        }

        section_size = s.VirtualSize;

        if (s.SizeOfRawData < section_size)
            section_size = s.SizeOfRawData;

        if (section_size > 0 && s.PointerToRawData != 0)
            memcpy(addr + s.VirtualAddress, (uint8_t*)&dos_header + s.PointerToRawData, section_size);

        if (section_size < s.VirtualSize) // if short, pad with zeroes
            memset(addr + s.VirtualAddress + section_size, 0, s.VirtualSize - section_size);
    }

    entry_point = img_addr + pe_header.OptionalHeader64.AddressOfEntryPoint;

    if (!do_relocations(pe_header, addr, img_addr))
        return false;

    return true;
}

static void load_initrd() {
    auto size = *ptr_initrd_size;

    if (size == 0)
        return;

    auto pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

    auto addr = (uint8_t*)get_pages(mem_type::initrd, pages);

    memcpy(addr, ptr_initrd_data, size);

    initrd_addr = (uintptr_t)addr;
}

static void copy_e820_entries() {
    for (unsigned int i = 0; i < *ptr_e820_count; i++) {
        const auto& e = ptr_e820_entries[i];
        auto& n = mem_entries[i];

        n.addr = e.address;
        n.len = e.length;

        switch (e.type) {
            case e820_mem_type::free:
                n.type = mem_type::free;
                break;
            case e820_mem_type::acpi_reclaimable:
            case e820_mem_type::acpi_nvs:
                n.type = mem_type::acpi;
                break;
            default:
                n.type = mem_type::reserved;
                break;
        }
    }

    mem_entries_count = *ptr_e820_count;
}

static void init_console() {
    text_console = span((uint16_t*)lb.text_console.phys_address,
                        lb.text_console.cols * lb.text_console.rows);

    memset(text_console.data(), 0, text_console.size_bytes());
}

static void text_console_overflow() {
    memcpy(text_console.data(), &text_console[lb.text_console.cols],
           (lb.text_console.rows - 1) * lb.text_console.cols * sizeof(uint16_t));
    memset(text_console.data() + ((lb.text_console.rows - 1) * lb.text_console.cols), 0,
           lb.text_console.cols * sizeof(uint16_t));
    cursor_y = lb.text_console.rows - 1;
}

static void text_console_putc(uint8_t x, uint8_t y, uint8_t c) {
    text_console[(y * lb.text_console.cols) + x] = (uint16_t)(((uint16_t)bg_colour << 12) | ((uint16_t)fg_colour << 8) | c);
}

static void print(string_view s) {
    if (!lb.text_mode)
        return;

    for (auto c : s) {
        if (c == '\n') {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == lb.text_console.rows)
                text_console_overflow();
        } else {
            text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, c);

            cursor_x++;

            if (cursor_x == lb.text_console.cols) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == lb.text_console.rows)
                    text_console_overflow();
            }
        }
    }
}

static bool search_for_acpi(span<uint8_t> data) {
    // FIXME - if both version 1 and version 2 present, should prefer version 2?

    while (data.size() >= 16) {
        if (*(uint64_t*)data.data() == RSD_PTR) {
            auto& rsdp = *(RSDPDescriptor*)data.data();

            if (rsdp.Revision != 0 && rsdp.Revision != 2)
                goto next;

            uint8_t sum = 0;

            for (unsigned int i = 0; i < sizeof(RSDPDescriptor); i++) {
                sum += data[i];
            }

            if (sum != 0)
                goto next;

            if (rsdp.Revision == 2) {
                auto& xsdp = *(XSDPDescriptor*)data.data();

                for (unsigned int i = sizeof(RSDPDescriptor); i <= sizeof(XSDPDescriptor); i++) {
                    sum += data[i];
                }

                if (sum != 0)
                    goto next;

                rsdt_address = xsdp.XsdtAddress;
            } else
                rsdt_address = rsdp.RsdtAddress;

            rsdp_pa = (uint64_t)data.data();

            return true;
        }

next:
        data = data.subspan(16);
    }

    return false;
}

static bool find_acpi() {
    auto ebda = (uint8_t*)(*(uint16_t*)0x40e * 0x10);

    if (search_for_acpi(span(ebda, 1024)))
        return true;

    if (search_for_acpi(span((uint8_t*)0xe0000, 0x20000)))
        return true;

    return false;
}

static void set_graphics_info() {
    auto& mode = ptr_vbe_modes[pm_vbe_selected_mode];

    if (mode.memory_model == MM_TEXT) {
        lb.text_mode = true;
        lb.text_console.cols = mode.width;
        lb.text_console.rows = mode.height;
        lb.text_console.phys_address = mode.segment_a << 4;
    } else {
        lb.text_mode = false;
        lb.framebuffer.phys_address = mode.framebuffer;
        lb.framebuffer.width = mode.width;
        lb.framebuffer.height = mode.height;
        lb.framebuffer.pixels_per_scan_line = mode.pitch / sizeof(uint32_t);

        if (*ptr_vbe_edid_valid)
            lb.edid = (uintptr_t)ptr_vbe_edid;
    }
}

extern "C"
void protected_mode_main() {
    if (pm_vbe_selected_mode == 0xffff) {
        lb.text_mode = true;
        lb.text_console.cols = 80;
        lb.text_console.rows = 25;
        lb.text_console.phys_address = 0xb8000;
    } else
        set_graphics_info();

    if (lb.text_mode)
        init_console();

    print("Starting bootloader\n");

    if (!have_long_mode()) {
        print("Long mode not supported.\n");
        goto halt;
    }

    copy_e820_entries();

    sanitize_mem_entries();
    mark_bootloader_memory();

    // as early as possible, as it could be in "free" memory
    load_initrd();

    setup_gdt();

    if (!find_acpi())
        print("Unable to find ACPI tables\n");

    if (!load_payload()) {
        print("load_payload failed\n");
        goto halt;
    }

    enable_paging();

    lb.map_ptr = (uintptr_t)mem_entries;
    lb.num_map_entries = mem_entries_count;
    lb.rsdp_pa = rsdp_pa;
    lb.cmdline = (uintptr_t)ptr_cmdline;
    lb.initrd_data = initrd_addr;
    lb.initrd_size = *ptr_initrd_size;
    lb.debug_info_ptr = debug_info_ptr;
    lb.debug_info_size = debug_info_size;
    lb.debug_abbrev_ptr = debug_abbrev_ptr;
    lb.debug_abbrev_size = debug_abbrev_size;
    lb.debug_rnglists_ptr = debug_rnglists_ptr;
    lb.debug_rnglists_size = debug_rnglists_size;
    lb.debug_str_ptr = debug_str_ptr;
    lb.debug_str_size = debug_str_size;
    lb.debug_addr_ptr = debug_addr_ptr;
    lb.debug_addr_size = debug_addr_size;
    lb.debug_str_offsets_ptr = debug_str_offsets_ptr;
    lb.debug_str_offsets_size = debug_str_offsets_size;
    lb.debug_line_ptr = debug_line_ptr;
    lb.debug_line_size = debug_line_size;
    lb.debug_line_str_ptr = debug_line_str_ptr;
    lb.debug_line_str_size = debug_line_str_size;

    // jump to entry point of image

    __asm__ __volatile__ (
        "push 0x18\n\t"
        "lea eax, long_mode_start\n\t"
        "push eax\n\t"
        "retf\n\t"
    );

halt:
    while (true) { }
}
