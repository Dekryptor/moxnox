#include <stdbool.h>
#include <stdint.h>
#include <intrin.h>

#include <span>

using namespace std;

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define CR0_PG      0x80000000

#define CR4_PAE     0x00000020

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
    uint16_t Offset;
    uint16_t Selector;
    uint16_t Access;
    uint16_t ExtendedOffset;
};

using pte_span = span<HARDWARE_PTE_PAE, PAGE_SIZE / sizeof(HARDWARE_PTE_PAE)>;

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

static uint32_t get_page() {
    uint32_t ret = next_page;

    next_page += PAGE_SIZE;

    return ret;
}

static void map_page(pte_span pt2, uint32_t phys, uint32_t virt) {
    auto& pt2_ent = pt2[virt >> 30];

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

    for (uint32_t page = 0; page < 0x200000; page += PAGE_SIZE) { // FIXME
        map_page(pd, page, page);
    }

    // enable PAE
    __writecr4(__readcr4() | CR4_PAE);

    // set cr3
    __writecr3((uintptr_t)pd.data());

    // enable paging
    __writecr0(__readcr0() | CR0_PG);
}

static void setup_idt() {
    GDTIDT desc;

    auto page = get_page();

    memset((void*)(uintptr_t)page, 0, PAGE_SIZE);

    desc.Base = (uintptr_t)page;
    desc.Limit = (NUM_IDT * sizeof(idt_entry)) - 1;

    __asm__ __volatile__ (
        "lidt %0\n\t"
        :
        : "m" (desc)
    );
}

extern "C"
void protected_mode_main() {
    setup_idt();

    enable_paging();

    while (true) {
    }
}
