#include <stdbool.h>
#include <stdint.h>
#include <intrin.h>

#include <span>

using namespace std;

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define CR0_PG      0x80000000

#define CR4_PAE     0x00000020

#define EFER_MSR    0xc0000080

#define EFER_LME    (1 << 8)

struct HARDWARE_PTE_PAE {
    uint64_t Valid:1;
    uint64_t Write:1;
    uint64_t Owner:1;
    uint64_t WriteThrough:1;
    uint64_t CacheDisable:1;
    uint64_t Accessed:1;
    uint64_t Dirty:1;
    uint64_t LargePage:1;
    uint64_t Global:1;
    uint64_t CopyOnWrite:1;
    uint64_t Prototype:1;
    uint64_t reserved0:1;
    uint64_t PageFrameNumber:28;
    uint64_t reserved1:12;
    uint64_t SoftwareWsIndex:11;
    uint64_t NoExecute:1;
};

#define NUM_IDT 256

struct GDTIDT {
    uint16_t Limit;
    uint32_t Base;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attributes;
    uint16_t offset_2;
} __attribute__((packed));

struct gdt_entry {
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint32_t BaseMid:8;
    uint32_t Type:5;
    uint32_t Dpl:2;
    uint32_t Pres:1;
    uint32_t LimitHi:4;
    uint32_t Sys:1;
    uint32_t Long:1;
    uint32_t Default_Big:1;
    uint32_t Granularity:1;
    uint32_t BaseHi:8;
} __attribute__((packed));

using pte_span = span<HARDWARE_PTE_PAE, PAGE_SIZE / sizeof(HARDWARE_PTE_PAE)>;

enum class mem_type : uint32_t {
    free = 1,
    reserved = 2,
    acpi_reclaimable = 3,
    acpi_nvs = 4,
    bad = 5,
    max_bios = bad
};

struct e820_entry {
    uint64_t address;
    uint64_t length;
    mem_type type;
} __attribute__((packed));

extern e820_entry* ptr_e820_entries;
extern uint32_t* ptr_e820_count;

static e820_entry* mem_entries;
static uint32_t mem_entries_count;

static uint32_t next_page = 0x200000; // FIXME - do this properly

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

void* memmove(void* dest, const void* src, size_t n) {
    auto orig_dest = dest;

    while (n > 0) {
        *(uint8_t*)dest = *(uint8_t*)src;

        dest = (uint8_t*)dest + 1;
        src = (uint8_t*)src + 1;

        n--;
    }

    return orig_dest;
}

static uint32_t get_page() {
    uint32_t ret = next_page;

    next_page += PAGE_SIZE;

    return ret;
}

static void map_page(pte_span pt3, uint64_t phys, uint64_t virt) {
    auto& pt3_ent = pt3[(virt >> 39) & 0x1ff];

    if (!pt3_ent.Valid) {
        pt3_ent.Valid = 1;
        pt3_ent.PageFrameNumber = get_page() >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt3_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt2((HARDWARE_PTE_PAE*)(uintptr_t)(pt3_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto& pt2_ent = pt2[(virt >> 30) & 0x1ff];

    if (!pt2_ent.Valid) {
        pt2_ent.Valid = 1;
        pt2_ent.Write = 1;
        pt2_ent.PageFrameNumber = get_page() >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt2_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt1((HARDWARE_PTE_PAE*)(uintptr_t)(pt2_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto &pt1_ent = pt1[(virt >> 21) & 0x1ff];

    if (!pt1_ent.Valid) {
        pt1_ent.Valid = 1;
        pt1_ent.Write = 1;
        pt1_ent.PageFrameNumber = get_page() >> PAGE_SHIFT;

        memset((void*)(uintptr_t)(pt1_ent.PageFrameNumber << PAGE_SHIFT), 0, PAGE_SIZE);
    }

    pte_span pt0((HARDWARE_PTE_PAE*)(uintptr_t)(pt1_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    auto &pt0_ent = pt0[(virt >> 12) & 0x1ff];

    pt0_ent.Valid = 1;
    pt0_ent.Write = 1;
    pt0_ent.PageFrameNumber = phys >> PAGE_SHIFT;
}

static void enable_paging() {
    pte_span pd((HARDWARE_PTE_PAE*)(uintptr_t)get_page(), PAGE_SIZE / sizeof(HARDWARE_PTE_PAE));

    memset(pd.data(), 0, pd.size_bytes());

    // FIXME - identity-map protected mode code and everything before it
    // FIXME - don't hardcode image base here

    for (uint32_t page = 0; page < next_page; page += PAGE_SIZE) { // FIXME
        map_page(pd, page, page);
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

weak_ordering operator<=>(enum mem_type m1, enum mem_type m2) {
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

static void add_new_mem_entry(unsigned int pos) {
    for (int i = mem_entries_count; i >= pos; i--) {
        mem_entries[i + 1] = mem_entries[i];
    }
    mem_entries_count++;
}

static void remove_mem_entry(unsigned int pos) {
    memmove(&mem_entries[pos], &mem_entries[pos + 1],
            (mem_entries_count - pos - 1) * sizeof(e820_entry));
    mem_entries_count--;
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

        for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
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
            auto cmp = a.type <=> b.type;

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

    // FIXME - combine adjacent
    // FIXME - watch out for last page
}

extern "C"
void protected_mode_main() {
    mem_entries = ptr_e820_entries;
    mem_entries_count = *ptr_e820_count;

    sanitize_mem_entries();

    setup_gdt();
    setup_idt();

    enable_paging();

    // FIXME - rm 32-bit IDT
    // FIXME - load PE payload into memory

    __asm__ __volatile__ (
        "push 0x18\n\t"
        "push OFFSET long_mode_start\n\t"
        "retf\n\t"
    );
}
