#include <stdbool.h>
#include <stdint.h>
#include <intrin.h>

#include <span>

using namespace std;

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define CR0_PG      0x80000000

#define CR4_PAE     0x00000020

struct HARDWARE_PTE {
    uint32_t Valid:1;
    uint32_t Write:1;
    uint32_t Owner:1;
    uint32_t WriteThrough:1;
    uint32_t CacheDisable:1;
    uint32_t Accessed:1;
    uint32_t Dirty:1;
    uint32_t LargePage:1;
    uint32_t Global:1;
    uint32_t CopyOnWrite:1;
    uint32_t Prototype:1;
    uint32_t Reserved:1;
    uint32_t PageFrameNumber:20;
};

using pte_span = span<HARDWARE_PTE, PAGE_SIZE / sizeof(HARDWARE_PTE)>;

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

static void identity_map_page(pte_span pd, uint32_t page) {
    auto& pd_ent = pd[page >> 22];

    if (!pd_ent.Valid) {
        pd_ent.Valid = 1;
        pd_ent.Write = 1;
        pd_ent.PageFrameNumber = get_page() >> PAGE_SHIFT;
    }

    pte_span pt((HARDWARE_PTE*)(uintptr_t)(pd_ent.PageFrameNumber << PAGE_SHIFT), PAGE_SIZE / sizeof(HARDWARE_PTE));

    auto &pt_ent = pt[(page >> 12) & 0x3ff];

    pt_ent.Valid = 1;
    pt_ent.Write = 1;
    pt_ent.PageFrameNumber = page >> PAGE_SHIFT;
}

static void enable_paging() {
    pte_span pd((HARDWARE_PTE*)(uintptr_t)get_page(), PAGE_SIZE / sizeof(HARDWARE_PTE));

    memset(pd.data(), 0, pd.size_bytes());

    // FIXME - identity-map protected mode code and everything before it
    // FIXME - don't hardcode image base here

    for (uint32_t page = 0; page < 0x200000; page += PAGE_SIZE) { // FIXME
        identity_map_page(pd, page);
    }

    // disable PAE
    __writecr4(__readcr4() & ~CR4_PAE);

    // set cr3
    __writecr3((uintptr_t)pd.data());

    // enable paging
    __writecr0(__readcr0() | CR0_PG);
}

extern "C"
void protected_mode_main() {
    enable_paging();

    while (true) {
    }
}
