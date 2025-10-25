module;

#include <string_view>
#include <span>
#include <cstring>
#include <bit>
#include <algorithm>
#include <mutex>
#include "assert.h"

#ifdef __aarch64__
#include "mem-aarch64.h"
#else
#include "mem-x86.h"
#endif

export module mem;

import config;
import list;
import print;
import format;
import bsod;
import spinlock;
import pe;
import loaderblock;

extern "C" uint8_t __image_base__;

#ifndef __aarch64__
// x86/mem.S
export {
    extern "C" void memcpy_aligned(void* dest, const void* src, size_t n);
    extern "C" void init_pat();
}
#endif

using namespace std;

export enum class mapping_type {
    no_access,
    read_write,
    read_only,
    read_execute,
    read_write_execute,
    no_cache,
    write_combine,
    reserved
};

export enum class page_state : uint8_t {
    free,
    used,
    folio_following,
    allocator_8k,
    allocator_4k,
    allocator_2k,
    allocator_1k,
    allocator_512,
    allocator_256,
    allocator_128,
    allocator_64
};

struct page {
    page_state state;
    uint8_t order;

    union {
        struct {
            list_entry le;
        } free;
        struct {
            uint64_t refcount;
        } used;
        struct {
            void* cb;
            unsigned int index;
        } allocator;
    };
};

static_assert(sizeof(struct page) == 24);

page* pages = (page*)0xfffffa0000000000;
static uint64_t total_pages;

static _spinlock<6> pa_spinlock;

static uint64_t* const vablock = (uint64_t*)0xfffffa4000000000;
static uint64_t vablock_size;

static uint8_t* const initrd_addr = (uint8_t*)0xfffffa5000000000;

static const uint64_t UEFI_VABLOCK = 0xfffffa6000000000;
static _EFI_MEMORY_DESCRIPTOR uefi_mappings[256];

static spinlock va_spinlock;
spinlock mapping_spinlock;
static spinlock protect_page_spinlock;

export span<const uint8_t> initrd;

constexpr uint64_t PHYS_MEMORY_MASK = 0xffffff0000000000;
static const uint64_t PHYS_MEMORY = 0xffff990000000000;

export constexpr uint8_t MAX_FOLIO_ORDER = 9; // 2MB

static list_entry folio_free_lists[MAX_FOLIO_ORDER + 1];
static uint64_t free_folios[MAX_FOLIO_ORDER + 1];

#ifndef __aarch64__
uint8_t MAXPHYADDR = 36;
#endif

export uint64_t zero_pfn;

template<unsigned N, enum page_state S>
class allocator_folio {
public:
    template<bool no_lock> void* malloc();
    template<bool no_lock> void free(void* ptr);
};

template<unsigned N, unsigned P, enum page_state S>
struct allocator_cb {
    allocator_cb() {
        memset(bitmap, 0, sizeof(bitmap));
    }

    template<bool no_lock> uint64_t malloc_pa();
    template<bool no_lock> void free(uint64_t index);

    static constexpr unsigned int NUM_PAGES = N;
    static constexpr unsigned int PAGES_PER_BITMAP_BLOCK = P;
    static constexpr enum page_state page_state_type = S;

    list_entry le;
    uint64_t bitmap[NUM_PAGES / PAGES_PER_BITMAP_BLOCK];
    uint64_t pfns[NUM_PAGES];
};

template<unsigned N, unsigned P, enum page_state S>
class allocator {
public:
    template<bool no_lock> void* malloc();
    template<bool no_lock> void free(void* ptr);

    using cb_type = allocator_cb<N, P, S>;

    _spinlock<6> lock;
    list_entry control_blocks = { &control_blocks, &control_blocks };
};

allocator_folio<1, page_state::allocator_8k> alloc8k;
allocator_folio<0, page_state::allocator_4k> alloc4k;
::allocator<488, 32, page_state::allocator_2k> alloc2k;
::allocator<480, 16, page_state::allocator_1k> alloc1k;
::allocator<448, 8, page_state::allocator_512> alloc512;
::allocator<408, 4, page_state::allocator_256> alloc256;
::allocator<336, 2, page_state::allocator_128> alloc128;
::allocator<255, 1, page_state::allocator_64> alloc64;
// FIXME - alloc32, alloc16, alloc8?

static_assert(sizeof(decltype(alloc2k)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc1k)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc512)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc256)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc128)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc64)::cb_type) <= PAGE_SIZE);

static bool map_uefi(const loader_block& lb);

export inline void* phys_address(uint64_t pa) {
    return (void*)(PHYS_MEMORY | pa);
}

static bool treat_as_free(enum mem_type type) {
    switch (type) {
        case mem_type::free:
        case mem_type::bootloader:
        case mem_type::paging_table:
            return true;

        default:
            return false;
    }
}

export
extern "C"
void* memset(void* s, int c, size_t n) {
    void* orig_s = s;

    // FIXME - do SSE version of this

    uint32_t v;

    v = 0;

    for (unsigned int i = 0; i < sizeof(uint32_t); i++) {
        v <<= 8;
        v |= c & 0xff;
    }

    while (n >= sizeof(uint32_t)) {
        *(__unaligned uint32_t*)s = v;

        s = (uint8_t*)s + sizeof(uint32_t);
        n -= sizeof(uint32_t);
    }

    while (n > 0) {
        *(uint8_t*)s = (uint8_t)c;

        s = (uint8_t*)s + 1;
        n--;
    }

    return orig_s;
}

export
extern "C"
int memcmp(const void* s1, const void* s2, size_t n) {
    // FIXME - not completely accurate, as 1 or -1 is messed up by endianness

    while (n > sizeof(uint64_t)) {
        uint64_t c1 = *(__unaligned uint64_t*)s1;
        uint64_t c2 = *(__unaligned uint64_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint64_t*)s1 + 1;
        s2 = (uint64_t*)s2 + 1;
        n -= sizeof(uint64_t);
    }

    while (n > sizeof(uint32_t)) {
        uint32_t c1 = *(__unaligned uint32_t*)s1;
        uint32_t c2 = *(__unaligned uint32_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint32_t*)s1 + 1;
        s2 = (uint32_t*)s2 + 1;
        n -= sizeof(uint32_t);
    }

    while (n > sizeof(uint16_t)) {
        uint16_t c1 = *(__unaligned uint16_t*)s1;
        uint16_t c2 = *(__unaligned uint16_t*)s2;

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

template<>
consteval bool format_spec_valid<enum page_state>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum page_state arg) {
    switch (arg) {
        case page_state::free:
            return do_format_arg(sp, "", "free");

        case page_state::used:
            return do_format_arg(sp, "", "used");

        case page_state::folio_following:
            return do_format_arg(sp, "", "folio_following");

        case page_state::allocator_8k:
            return do_format_arg(sp, "", "allocator_8k");

        case page_state::allocator_4k:
            return do_format_arg(sp, "", "allocator_4k");

        case page_state::allocator_2k:
            return do_format_arg(sp, "", "allocator_2k");

        case page_state::allocator_1k:
            return do_format_arg(sp, "", "allocator_1k");

        case page_state::allocator_512:
            return do_format_arg(sp, "", "allocator_512");

        case page_state::allocator_256:
            return do_format_arg(sp, "", "allocator_256");

        case page_state::allocator_128:
            return do_format_arg(sp, "", "allocator_128");

        case page_state::allocator_64:
            return do_format_arg(sp, "", "allocator_64");

        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

#ifdef __aarch64__
export template<>
consteval bool format_spec_valid<ARM64_HARDWARE_PTE>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const ARM64_HARDWARE_PTE& p) {
    sp = do_format_arg(sp, "", "valid=");
    sp = do_format_arg(sp, "", p.valid);
    sp = do_format_arg(sp, "", ", table_entry=");
    sp = do_format_arg(sp, "", p.table_entry);
    sp = do_format_arg(sp, "", ", AttrIndex=");
    sp = do_format_arg(sp, "", p.AttrIndex);
    sp = do_format_arg(sp, "", ", NS=");
    sp = do_format_arg(sp, "", p.NS);
    sp = do_format_arg(sp, "", ", AP=");
    sp = do_format_arg(sp, "", p.AP);
    sp = do_format_arg(sp, "", ", SH=");
    sp = do_format_arg(sp, "", p.SH);
    sp = do_format_arg(sp, "", ", AF=");
    sp = do_format_arg(sp, "", p.AF);
    sp = do_format_arg(sp, "", ", nG=");
    sp = do_format_arg(sp, "", p.nG);
    sp = do_format_arg(sp, "", ", pfn=");
    sp = do_format_arg(sp, ":x", p.pfn);
    sp = do_format_arg(sp, "", ", DBM=");
    sp = do_format_arg(sp, "", p.DBM);
    sp = do_format_arg(sp, "", ", Contig=");
    sp = do_format_arg(sp, "", p.Contig);
    sp = do_format_arg(sp, "", ", PXN=");
    sp = do_format_arg(sp, "", p.PXN);
    sp = do_format_arg(sp, "", ", UXN=");
    sp = do_format_arg(sp, "", p.UXN);
    sp = do_format_arg(sp, "", ", allow_write=");
    sp = do_format_arg(sp, "", p.allow_write);
    sp = do_format_arg(sp, "", ", cow=");
    sp = do_format_arg(sp, "", p.cow);
    sp = do_format_arg(sp, "", ", reserved=");
    sp = do_format_arg(sp, "", p.reserved);
    sp = do_format_arg(sp, "", ", software=");
    sp = do_format_arg(sp, "", p.software);
    sp = do_format_arg(sp, "", ", imp_def=");
    sp = do_format_arg(sp, "", p.imp_def);

    return sp;
}

export uint64_t ttbr0_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], ttbr0_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

export uint64_t ttbr1_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], ttbr1_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

static bool map_page_with_scratch(uint64_t va, uint64_t pa, uint64_t& scratch_pa, uint64_t& scratch_pages) {
    auto pte0 = span((ARM64_HARDWARE_PTE*)ttbr1_el1_load(), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p0 = pte0[(va & 0xff8000000000) >> 39];

    if (!p0.valid) {
        if (scratch_pages == 0)
            return false;

        p0.valid = 1;
        p0.table_entry = 1;
        p0.AttrIndex = 0;
        p0.AP = ARM64_PTE_AP_EL1RW;
        p0.SH = 3; // inner shareable
        p0.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p0.pfn = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

        memset((void*)(p0.pfn << PAGE_SHIFT), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto pte1 = span((ARM64_HARDWARE_PTE*)(p0.pfn << PAGE_SHIFT), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p1 = pte1[(va & 0x7fc0000000) >> 30];

    if (!p1.valid) {
        if (scratch_pages == 0)
            return false;

        p1.valid = 1;
        p1.table_entry = 1;
        p1.AttrIndex = 0;
        p1.AP = ARM64_PTE_AP_EL1RW;
        p1.SH = 3; // inner shareable
        p1.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p1.pfn = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

        memset((void*)(p1.pfn << PAGE_SHIFT), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto pte2 = span((ARM64_HARDWARE_PTE*)(p1.pfn << PAGE_SHIFT), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p2 = pte2[(va & 0x3fe00000) >> 21];

    if (!p2.valid) {
        if (scratch_pages == 0)
            return false;

        p2.valid = 1;
        p2.table_entry = 1;
        p2.AttrIndex = 0;
        p2.AP = ARM64_PTE_AP_EL1RW;
        p2.SH = 3; // inner shareable
        p2.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p2.pfn = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

        memset((void*)(p2.pfn << PAGE_SHIFT), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto pte3 = span((ARM64_HARDWARE_PTE*)(p2.pfn << PAGE_SHIFT), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p3 = pte3[(va & 0x1ff000) >> PAGE_SHIFT];

    p3.valid = 1;
    p3.table_entry = 1;
    p3.AttrIndex = 0;
    p3.AP = ARM64_PTE_AP_EL1RW;
    p3.SH = 3; // inner shareable
    p3.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    p3.pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

    return true;
}
#else
static bool map_page_with_scratch(uint64_t va, uint64_t pa, uint64_t& scratch_pa, uint64_t& scratch_pages) {
    auto pml4 = span((HARDWARE_PTE*)SELFMAP_PML4, 512);
    auto pdpt = span((HARDWARE_PTE*)(SELFMAP_PDPT | (((va & 0xff8000000000) >> 39) << PAGE_SHIFT)), 512);
    auto pdt = span((HARDWARE_PTE*)(SELFMAP_PDT | (((va & 0xffffc0000000) >> 30) << PAGE_SHIFT)), 512);
    auto pt = span((HARDWARE_PTE*)(SELFMAP | (((va & 0xffffffe00000) >> 21) << PAGE_SHIFT)), 512);

    auto& pml4_entry = pml4[(va & 0xff8000000000) >> 39];

    if (!pml4_entry.p) {
        if (scratch_pages == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pml4_entry.pfn = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pml4_entry.p = 1;
        pml4_entry.w = 1;

        memset(pdpt.data(), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto& pdpt_entry = pdpt[(va & 0x7fc0000000) >> 30];

    if (!pdpt_entry.p) {
        if (scratch_pages == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdpt_entry.pfn = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pdpt_entry.p = 1;
        pdpt_entry.w = 1;

        memset(pdt.data(), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto& pdt_entry = pdt[(va & 0x3fe00000) >> 21];

    if (!pdt_entry.p) {
        if (scratch_pages == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdt_entry.pfn = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pdt_entry.p = 1;
        pdt_entry.w = 1;

        memset(pt.data(), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto& pt_entry = pt[(va & 0x1ff000) >> PAGE_SHIFT];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pt_entry.pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pt_entry.p = 1;
    pt_entry.w = 1;
    pt_entry.xd = 1;

    return true;
}
#endif

static void fragment_folio(uint64_t pfn, uint8_t desired_order) {
    uint8_t order = pages[pfn].order;

    // break folio into smaller pieces

    free_folios[order]--;

    remove_entry_list(pages[pfn].free.le);

    for (unsigned int i = 0; i < (unsigned int)(order - desired_order); i++) {
        auto first_pfn = pfn;

        pages[pfn].state = page_state::free;

        auto new_order = (uint8_t)(order - i - 1);
        auto num_pages = (uint64_t)1 << new_order;

        for (uint64_t j = 0; j < num_pages; j++) {
            pages[pfn].order = new_order;
            pfn++;
        }

        insert_tail_list(folio_free_lists[new_order], pages[first_pfn].free.le);
        free_folios[new_order]++;
    }

    pages[pfn].state = page_state::free;
    pages[pfn].order = desired_order;
    insert_tail_list(folio_free_lists[desired_order], pages[pfn].free.le);
    free_folios[desired_order]++;

    for (uint64_t j = 1; j < (uint64_t)1 << desired_order; j++) {
        pages[pfn + j].order = desired_order;
    }
}

export uint64_t allocate_folio_no_lock(uint8_t order, enum page_state state) {
    for (unsigned int i = order; i <= MAX_FOLIO_ORDER; i++) {
        if (is_list_empty(folio_free_lists[i]))
            continue;

        auto& p = *(page*)((uint8_t*)folio_free_lists[i].next - offsetof(page, free.le));
        auto orig_pfn = (uint64_t)(&p - pages);
        uint64_t pfn;

        if (i == order)
            pfn = orig_pfn;
        else {
            fragment_folio(orig_pfn, order);

            pfn = orig_pfn + ((uint64_t)1 << i) - ((uint64_t)1 << order);
        }

        remove_entry_list(pages[pfn].free.le);
        pages[pfn].state = state;

        if (state == page_state::used)
            pages[pfn].used.refcount = 1;

        free_folios[order]--;

        return pfn << PAGE_SHIFT;
    }

    return 0;
}

export uint64_t allocate_folio(uint8_t order, enum page_state state = page_state::used) {
    lock_guard lock(pa_spinlock);

    return allocate_folio_no_lock(order, state);
}

export uint64_t allocate_page(enum page_state state = page_state::used) {
    return allocate_folio(0, state);
}

#ifndef __aarch64__
__attribute__((always_inline))
uint64_t inline __readcr3() {
    uint64_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr3"
        : "=r" (ret)
    );

    return ret;
}
#endif

#ifdef __aarch64__
export template<bool no_lock = false>
bool map_page(uint64_t pa, uint64_t va, enum mapping_type type) {
    // FIXME - userspace addresses (ttbr0, AP)

    auto func = [&]() {
        auto& p0 = *(ARM64_HARDWARE_PTE*)phys_address(ttbr1_el1_load() | (((va & 0xff8000000000) >> 39) << 3));

        if (!p0.valid) {
            uint64_t page;

            // FIXME - should there be an allocate_zeroed_page?
            if constexpr (no_lock)
                page = allocate_folio_no_lock(0, page_state::used);
            else
                page = allocate_page();

            if (page == 0)
                return false;

            p0.valid = 1;
            p0.table_entry = 1;
            p0.AttrIndex = 0;
            p0.AP = ARM64_PTE_AP_EL1RW;
            p0.SH = 3; // inner shareable
            p0.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            p0.pfn = page >> PAGE_SHIFT;
#pragma GCC diagnostic pop

            memset(phys_address(page), 0, PAGE_SIZE);
        }

        auto& p1 = *(ARM64_HARDWARE_PTE*)phys_address((p0.pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));

        if (!p1.valid) {
            uint64_t page;

            if constexpr (no_lock)
                page = allocate_folio_no_lock(0, page_state::used);
            else
                page = allocate_page();

            if (page == 0)
                return false;

            p1.valid = 1;
            p1.table_entry = 1;
            p1.AttrIndex = 0;
            p1.AP = ARM64_PTE_AP_EL1RW;
            p1.SH = 3; // inner shareable
            p1.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            p1.pfn = page >> PAGE_SHIFT;
#pragma GCC diagnostic pop

            memset(phys_address(page), 0, PAGE_SIZE);
        }

        auto& p2 = *(ARM64_HARDWARE_PTE*)phys_address((p1.pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));

        if (!p2.valid) {
            uint64_t page;

            if constexpr (no_lock)
                page = allocate_folio_no_lock(0, page_state::used);
            else
                page = allocate_page();

            if (page == 0)
                return false;

            p2.valid = 1;
            p2.table_entry = 1;
            p2.AttrIndex = 0;
            p2.AP = ARM64_PTE_AP_EL1RW;
            p2.SH = 3; // inner shareable
            p2.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            p2.pfn = page >> PAGE_SHIFT;
#pragma GCC diagnostic pop

            memset(phys_address(page), 0, PAGE_SIZE);
        }

        auto& p3 = *(ARM64_HARDWARE_PTE*)phys_address((p2.pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> 12) << 3));

        memset(&p3, 0, sizeof(p3));

        p3.valid = 1;
        p3.table_entry = 1;

        if (type == mapping_type::no_cache)
            p3.AttrIndex = 1; // nGnRnE device memory
        else
            p3.AttrIndex = 0; // normal memory

        p3.AP = ARM64_PTE_AP_EL1RW;
        p3.SH = 3; // inner shareable
        p3.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p3.pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

        // FIXME - type

        return true;
    };

    if constexpr (no_lock)
        return func();
    else {
        lock_guard lock(mapping_spinlock);
        return func();
    }
}
#else
export template<bool = false>
bool map_page(uint64_t pa, uint64_t va, enum mapping_type type) {
    if (pa >= (uint64_t)1 << MAXPHYADDR) // out of range
        return false;

    lock_guard lock(mapping_spinlock);

    auto pml4_entry_ptr = (volatile uint64_t*)phys_address((__readcr3() & 0xfffffffffffff000) | (((va & 0xff8000000000) >> 39) << 3));

    HARDWARE_PTE pml4_entry;

    *(uint64_t*)&pml4_entry = *pml4_entry_ptr;

    if (!pml4_entry.p) {
        uint64_t page = allocate_page(); // FIXME - should there be an allocate_zeroed_page?
        if (page == 0)
            return false;

        pml4_entry.pfn = (page >> PAGE_SHIFT) & 0x3ffffffffffff;
        pml4_entry.p = 1;
        pml4_entry.w = 1;
        pml4_entry.u = va & 0x8000000000000000 ? 0 : 1;

        memset(phys_address(page), 0, PAGE_SIZE);

        *pml4_entry_ptr = *(uint64_t*)&pml4_entry;
    }

    uint64_t pfn = pml4_entry.pfn;
    auto pdpt_entry_ptr = (volatile uint64_t*)phys_address((pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));

    HARDWARE_PTE pdpt_entry;

    *(uint64_t*)&pdpt_entry = *pdpt_entry_ptr;

    if (!pdpt_entry.p) {
        uint64_t page = allocate_page();
        if (page == 0)
            return false;

        pdpt_entry.pfn = (page >> PAGE_SHIFT) & 0x3ffffffffffff;
        pdpt_entry.p = 1;
        pdpt_entry.w = 1;
        pdpt_entry.u = va & 0x8000000000000000 ? 0 : 1;

        memset(phys_address(page), 0, PAGE_SIZE);

        *pdpt_entry_ptr = *(uint64_t*)&pdpt_entry;
    }

    pfn = pdpt_entry.pfn;
    auto pdt_entry_ptr = (volatile uint64_t*)phys_address((pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));

    HARDWARE_PTE pdt_entry;

    *(uint64_t*)&pdt_entry = *pdt_entry_ptr;

    if (!pdt_entry.p) {
        uint64_t page = allocate_page();
        if (page == 0)
            return false;

        pdt_entry.pfn = (page >> PAGE_SHIFT) & 0x3ffffffffffff;
        pdt_entry.p = 1;
        pdt_entry.w = 1;
        pdt_entry.u = va & 0x8000000000000000 ? 0 : 1;

        memset(phys_address(page), 0, PAGE_SIZE);

        *pdt_entry_ptr = *(uint64_t*)&pdt_entry;
    }

    pfn = pdt_entry.pfn;
    auto pt_entry_ptr = (volatile uint64_t*)phys_address((pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> 12) << 3));

    HARDWARE_PTE pt_entry;

    *(uint64_t*)&pt_entry = 0;

    pt_entry.pfn = (pa >> PAGE_SHIFT) & 0x3ffffffffffff;
    pt_entry.p = 1;

    switch (type) {
        case mapping_type::no_access:
            pt_entry.xd = 1;
        break;

        case mapping_type::read_write:
            pt_entry.xd = 1;
            pt_entry.w = 1;
        break;

        case mapping_type::read_only:
            pt_entry.xd = 1;
        break;

        case mapping_type::read_execute:
        break;

        case mapping_type::read_write_execute:
            pt_entry.w = 1;
        break;

        case mapping_type::no_cache:
            pt_entry.xd = 1;
            pt_entry.w = 1;
            pt_entry.pcd = 1;
        break;

        case mapping_type::write_combine:
            pt_entry.xd = 1;
            pt_entry.w = 1;
            pt_entry.ps = 1;
            pt_entry.pcd = 1;
        break;

        case mapping_type::reserved:
            return false;
    }

    pt_entry.u = (type == mapping_type::no_access || va & 0x8000000000000000) ? 0 : 1;

    *pt_entry_ptr = *(uint64_t*)&pt_entry;

    return true;
}
#endif

template bool map_page<true>(uint64_t pa, uint64_t va, enum mapping_type type);

static bool map_memblock(uint64_t pa, uint64_t num_pages, uint64_t& scratch_pages) {
    uint64_t scratch_pa = pa + (num_pages << PAGE_SHIFT);
    uint64_t va = (uintptr_t)pages;

    while (num_pages > 0) {
        if (!map_page_with_scratch(va, pa, scratch_pa, scratch_pages))
            return false;

        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        num_pages--;
    }

    return true;
}

static void mark_mem_as_free(uint64_t pa, size_t len, bool skip_spinlock) {
    uint64_t pfn = pa >> PAGE_SHIFT;
    uint64_t num_pages = len >> PAGE_SHIFT;

    auto func = [&]() {
        for (uint64_t i = 0; i < num_pages; i++) {
            if (pages[pfn].state != page_state::free) {
                insert_tail_list(folio_free_lists[0], pages[pfn].free.le);
                pages[pfn].state = page_state::free;
                free_folios[0]++;
            }

            pfn++;
        }
    };

    if (skip_spinlock)
        func();
    else {
        lock_guard lock(pa_spinlock);
        func();
    }
}

static void mark_mem_as_used(uint64_t pa, size_t len) {
    uint64_t pfn = pa >> PAGE_SHIFT;
    uint64_t num_pages = len >> PAGE_SHIFT;

    // not getting pa_spinlock as interrupts not setup yet

    for (uint64_t i = 0; i < num_pages; i++) {
        if (pages[pfn].state == page_state::free) {
            remove_entry_list(pages[pfn].free.le);
            pages[pfn].state = page_state::used;
            free_folios[0]--;
        }

        pfn++;
    }
}

#ifdef __aarch64__
export void free_lower_half_mappings() {
    auto ttbr0 = ttbr0_el1_load();
    auto pte0 = span((ARM64_HARDWARE_PTE*)phys_address(ttbr0), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

    for (auto& p1 : pte0) {
        if (!p1.valid)
            continue;

        auto pte1 = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)p1.pfn << PAGE_SHIFT), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

        for (auto& p2 : pte1) {
            if (!p2.valid || !p2.table_entry)
                continue;

            auto pte2 = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)p2.pfn << PAGE_SHIFT), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

            for (auto& p3 : pte2) {
                if (!p3.valid || !p3.table_entry)
                    continue;

                mark_mem_as_free((uint64_t)p3.pfn << PAGE_SHIFT, PAGE_SIZE, false);
            }

            mark_mem_as_free((uint64_t)p2.pfn << PAGE_SHIFT, PAGE_SIZE, false);
        }

        mark_mem_as_free((uint64_t)p1.pfn << PAGE_SHIFT, PAGE_SIZE, false);
    }

    mark_mem_as_free(ttbr0, PAGE_SIZE, false);
}
#else
export void free_lower_half_mappings() {
    auto cr3 = __readcr3();
    auto pml4 = span((HARDWARE_PTE*)phys_address(cr3 & ~(PAGE_SIZE - 1)), 512);

    for (unsigned int i = 0; i < 256; i++) {
        auto& p1 = pml4[i];

        if (p1.p) {
            auto pdpt = span((HARDWARE_PTE*)phys_address((uint64_t)p1.pfn << PAGE_SHIFT), 512);

            for (unsigned int j = 0; j < pdpt.size(); j++) {
                auto& p2 = pdpt[j];

                if (p2.p) {
                    auto pdt = span((HARDWARE_PTE*)phys_address((uint64_t)p2.pfn << PAGE_SHIFT), 512);

                    for (unsigned int k = 0; k < pdt.size(); k++) {
                        auto& p3 = pdt[k];

                        if (p3.p)
                            mark_mem_as_free((uint64_t)p3.pfn << PAGE_SHIFT, PAGE_SIZE, false);
                    }

                    mark_mem_as_free((uint64_t)p2.pfn << PAGE_SHIFT, PAGE_SIZE, false);
                }
            }

            mark_mem_as_free((uint64_t)p1.pfn << PAGE_SHIFT, PAGE_SIZE, false);
            *(uint64_t*)&p1 = 0;
        }
    }

    // make sure mappings updated
    __asm __volatile(
        "mov rax, cr3\n"
        "mov cr3, rax\n"
        :
        :
        : "rax"
    );
}
#endif

static uint8_t find_max_order(uint64_t pfn) {
    auto ret = (uint8_t)countr_zero(pfn);
    if (ret == 0)
        return 0;

    if (ret > MAX_FOLIO_ORDER)
        ret = MAX_FOLIO_ORDER;

    // make sure doesn't overflow end
    if (pfn + (1 << ret) >= total_pages) {
        auto tmp = (uint64_t)(total_pages - pfn);

        ret = (uint8_t)((sizeof(uint64_t) * 8) - (uint8_t)countl_zero(tmp) - 1);
    }

    return ret;
}

static uint64_t last_page;

export void setup_folios() {
    lock_guard lock(pa_spinlock);

    for (uint64_t i = 0; i < total_pages; i += 2) {
        last_page = i;

        if (pages[i].state != page_state::free)
            continue;

        auto max_order = find_max_order(i);
        if (max_order == 0)
            continue;

        uint64_t run_length = 1;

        for (uint64_t j = 1; j < (uint64_t)1 << max_order; j++) {
            if (pages[i + j].state == page_state::free)
                run_length++;
            else
                break;
        }

        if (run_length == 1)
            continue;

        auto order = (uint8_t)((sizeof(uint64_t) * 8) - (uint8_t)countl_zero(run_length) - 1);

        run_length = 1 << order;

        for (uint64_t j = 0; j < run_length; j++) {
            pages[i + j].order = order;

            if (j != 0)
                pages[i + j].state = page_state::folio_following;

            remove_entry_list(pages[i + j].free.le);
        }

        insert_tail_list(folio_free_lists[order], pages[i].free.le);

        free_folios[order]++;
        free_folios[0] -= 1 << order;

        i += run_length - 2;
    }
}

#ifdef __aarch64__
static bool map_physical_memory(uint64_t num_pages) {
    auto pte0 = span((ARM64_HARDWARE_PTE*)ttbr1_el1_load(), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p0 = pte0[(PHYS_MEMORY & 0xff8000000000) >> 39];

    auto pa = allocate_folio_no_lock(0, page_state::used);
    if (!pa) {
        print("allocate_folio_no_lock failed\n");
        return false;
    }

    p0.valid = 1;
    p0.table_entry = 1;
    p0.AttrIndex = 0;
    p0.AP = ARM64_PTE_AP_EL1RW;
    p0.SH = 3; // inner shareable
    p0.AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    p0.pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

    memset((void*)(p0.pfn << PAGE_SHIFT), 0, PAGE_SIZE);

    auto pte1 = span((ARM64_HARDWARE_PTE*)(p0.pfn << PAGE_SHIFT), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

    static const uint64_t PAGES_PER_GIGABYTE = 1024 * 1024 * 1024 / PAGE_SIZE;

    // FIXME - if more than 512GB, will need to allocate second p0 entry

    uint64_t gigabytes = (num_pages + PAGES_PER_GIGABYTE - 1) / PAGES_PER_GIGABYTE;
    auto* p1 = &pte1[(PHYS_MEMORY & 0x7fc0000000) >> 30];

    for (unsigned int i = 0; i < gigabytes; i++) {
        p1->valid = 1;
        p1->table_entry = 0;
        p1->AttrIndex = 0;
        p1->AP = ARM64_PTE_AP_EL1RW;
        p1->SH = 3; // inner shareable
        p1->AF = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p1->pfn = i * PAGES_PER_GIGABYTE;
#pragma GCC diagnostic pop

        p1++;
    }

    return true;
}
#else
static bool map_physical_memory(uint64_t num_pages) {
    auto pml4 = span((HARDWARE_PTE*)SELFMAP_PML4, 512);
    auto& pml4_entry = pml4[(PHYS_MEMORY & 0xff8000000000) >> 39];
    auto pdpt = span((HARDWARE_PTE*)(SELFMAP_PDPT | (((PHYS_MEMORY & 0xff8000000000) >> 39) << PAGE_SHIFT)), 512);

    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return false;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pml4_entry.pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pml4_entry.p = 1;
    pml4_entry.w = 1;

    memset(pdpt.data(), 0, pdpt.size_bytes());

    static const uint64_t PAGES_PER_GIGABYTE = 1024 * 1024 * 1024 / 4096;

    // FIXME - if more than 512GB, will need to allocate second pml4 entry

    uint64_t gigabytes = (num_pages + PAGES_PER_GIGABYTE - 1) / PAGES_PER_GIGABYTE;
    auto* pdpt_entry = &pdpt[(PHYS_MEMORY & 0x7fc0000000) >> 30];

    for (unsigned int i = 0; i < gigabytes; i++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdpt_entry->pfn = i * PAGES_PER_GIGABYTE;
#pragma GCC diagnostic pop
        pdpt_entry->p = 1;
        pdpt_entry->w = 1;
        pdpt_entry->ps = 1;

        pdpt_entry++;
    }

    return true;
}
#endif

export constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static bool load_initrd(void* data, uintptr_t size) {
    auto num_pages = bytes_to_pages(size);

    for (unsigned int i = 0; i < num_pages; i++) {
        auto pa = allocate_folio_no_lock(0, page_state::used);
        if (!pa)
            bsod("allocate_page failed in load_initrd");

        if (!map_page<true>(pa, (uintptr_t)initrd_addr + (i << PAGE_SHIFT), mapping_type::read_write))
            bsod("map_page failed in load_initrd");
    }

    memcpy(initrd_addr, data, size);

    initrd = span(initrd_addr, size);

    // FIXME - should free this memory once boot is done

    return true;
}

#ifndef __aarch64__
static uint8_t get_maxphyaddr() {
    uint32_t eax, ebx, ecx, edx;

    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (0x80000000)
    );

    if (eax < 0x80000008) // memory cpuid not supported
        return 36;

    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (0x80000008)
    );

    return (uint8_t)(eax & 0xff);
}

#endif

export bool init_memory(const loader_block& lb) {
    uint64_t num_memblock_bytes, num_memblock_pages, orig_scratch_pages, scratch_pages, needed_pages;
    unsigned int num_map_entries = lb.num_map_entries;
    uint64_t memblock_pa;
    auto lb_map = (map_entry*)lb.map_ptr;

    for (unsigned int i = 0; i < MAX_FOLIO_ORDER + 1; i++) {
        folio_free_lists[i].next = &folio_free_lists[i];
        folio_free_lists[i].prev = &folio_free_lists[i];
    }

#ifndef __aarch64__
    init_pat();

    MAXPHYADDR = get_maxphyaddr();
#endif

    // ignore any non-free entries at end of table
    while (num_map_entries > 0 && !treat_as_free(lb_map[num_map_entries - 1].type)) {
        num_map_entries--;
    }

    if (num_map_entries == 0)
        return false;

    auto map_entries = span(lb_map, num_map_entries);

    auto& m = map_entries[num_map_entries - 1];
    total_pages = (m.addr + m.len) >> PAGE_SHIFT;

    num_memblock_bytes = total_pages * sizeof(page);
    num_memblock_pages = bytes_to_pages(num_memblock_bytes);

    orig_scratch_pages = 5; // pages needed for paging tables

    orig_scratch_pages += num_memblock_pages / 512; // for second-level pages
    orig_scratch_pages += num_memblock_pages / (512 * 512); // for third-level pages

    scratch_pages = orig_scratch_pages;
    needed_pages = num_memblock_pages + scratch_pages;

    // allocate physical pages for memblock

    memblock_pa = 0;
    for (auto& map : map_entries) {
        if (map.type == mem_type::free && map.len >= (needed_pages << PAGE_SHIFT)) {
            memblock_pa = map.addr;
            break;
        }
    }

    if (memblock_pa == 0)
        return false;

    if (!map_memblock(memblock_pa, num_memblock_pages, scratch_pages))
        return false;

    // populate memblock from loader block

    for (uint64_t i = 0; i < total_pages; i++) {
        pages[i].state = page_state::used;
        pages[i].order = 0;
    }

    for (auto& map : map_entries) {
        // bootloader memory also gets marked as free (inc. page tables)
        if (map.type == mem_type::free || map.type == mem_type::bootloader)
            mark_mem_as_free(map.addr, map.len, true);
    }

    // subtract memblock itself, and scratch that we've used
    mark_mem_as_used(memblock_pa, (num_memblock_pages + (orig_scratch_pages - scratch_pages)) << PAGE_SHIFT);

    mark_mem_as_used(0, PAGE_SIZE);

    if (lb.text_mode) {
        size_t bytes = lb.text_console.cols * lb.text_console.rows * sizeof(uint16_t);

        mark_mem_as_used(lb.text_console.phys_address,
                         bytes_to_pages(bytes) << PAGE_SHIFT);
    }

    // FIXME - probably should double-check that framebuffer memory is marked used

    // make sure that at least the first 4GB of physical memory are mapped,
    // because the ACPI tables live there (is this guaranteed?)
    // FIXME - non-EFI bootloader should walk the ACPI tables and mark the memory
    //         as mem_type::acpi (Seabios E820 reports it as reserved)
    map_physical_memory(max(total_pages, (uint64_t)(1 << (32 - PAGE_SHIFT))));

    if (lb.initrd_size != 0) {
        load_initrd((void*)lb.initrd_data, lb.initrd_size);

        // FIXME - mark source memory as free (is page-aligned)
    }

    zero_pfn = allocate_folio_no_lock(0, page_state::used) >> PAGE_SHIFT;
    if (!zero_pfn) {
        print("could not allocate zero page\n");
        return false;
    }

    memset(phys_address(zero_pfn << PAGE_SHIFT), 0, PAGE_SIZE);

    if (lb.SetVirtualAddressMap) {
        if (!map_uefi(lb))
            return false;
    }

    return true;
}

export template<bool no_lock = false>
void* malloc_aligned(size_t size, size_t align) {
    if (size == 0)
        return nullptr;

    if (size < align)
        size = align;

    if (size <= 64)
        return alloc64.malloc<no_lock>();
    else if (size <= 128)
        return alloc128.malloc<no_lock>();
    else if (size <= 256)
        return alloc256.malloc<no_lock>();
    else if (size <= 512)
        return alloc512.malloc<no_lock>();
    else if (size <= 1024)
        return alloc1k.malloc<no_lock>();
    else if (size <= 2048)
        return alloc2k.malloc<no_lock>();
    else if (size <= 4096)
        return alloc4k.malloc<no_lock>();
    else if (size <= 8192)
        return alloc8k.malloc<no_lock>();
    else {
        bsod_format("malloc asked to do over-large allocation ({:x} bytes)\n", size);
        return nullptr;
    }
}

template void* malloc_aligned<true>(size_t size, size_t align);

export
extern "C"
void* malloc(size_t size) {
    return malloc_aligned(size, 1);
}

export
extern "C"
void* calloc(size_t nmemb, size_t size) {
    // FIXME - should test for overflow of nmemb * size

    auto ptr = malloc(nmemb * size);
    if (!ptr)
        return nullptr;

    memset(ptr, 0, nmemb * size);

    return ptr;
}

static constexpr bool is_phys_memory(void* ptr) {
    return ((uintptr_t)ptr & PHYS_MEMORY_MASK) == PHYS_MEMORY;
}

export constexpr uint64_t phys_memory_pfn(void* ptr) {
    return ((uintptr_t)ptr & ~PHYS_MEMORY_MASK) >> PAGE_SHIFT;
}

export constexpr uint64_t va_to_pa_phys(void* ptr) {
    return (phys_memory_pfn(ptr) << PAGE_SHIFT) | ((uintptr_t)ptr & (PAGE_SIZE - 1));
}

template<bool no_lock>
static void free2(void* ptr) {
    if (!ptr)
        return;

    if (is_phys_memory(ptr)) {
        auto& page = pages[phys_memory_pfn(ptr)];

        switch (page.state) {
            case page_state::allocator_8k:
                alloc8k.free<no_lock>(ptr);
                return;

            case page_state::allocator_4k:
                alloc4k.free<no_lock>(ptr);
                return;

            case page_state::allocator_2k:
                alloc2k.free<no_lock>(ptr);
                return;

            case page_state::allocator_1k:
                alloc1k.free<no_lock>(ptr);
                return;

            case page_state::allocator_512:
                alloc512.free<no_lock>(ptr);
                return;

            case page_state::allocator_256:
                alloc256.free<no_lock>(ptr);
                return;

            case page_state::allocator_128:
                alloc128.free<no_lock>(ptr);
                return;

            case page_state::allocator_64:
                alloc64.free<no_lock>(ptr);
                return;

            default:
                bsod_format("free called for unexpected page state {}", page.state);
        }
    } else
        bsod_format("free called for non-physical address {:x}", (uintptr_t)ptr);
}

export void free_no_lock(void* ptr) {
    free2<true>(ptr);
}

export
extern "C"
void free(void* ptr) {
    free2<false>(ptr);
}

export
extern "C++"
void* operator new(size_t count) {
    return malloc_aligned(count, 1);
}

export
extern "C++"
void* operator new(size_t count, align_val_t al) {
    return malloc_aligned(count, (size_t)al);
}

export
extern "C++"
void operator delete(void* ptr) noexcept {
    free(ptr);
}

export
extern "C++"
void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

static void try_to_compact(uint64_t pfn) {
    while (true) {
        uint64_t first_pfn, second_pfn;
        uint8_t order = pages[pfn].order;

        if (order == MAX_FOLIO_ORDER)
            return;

        auto num_pages = (uint64_t)1 << order;

        if (pfn & ((uint64_t)1 << order)) { // compact left
            if (pfn < num_pages)
                return;

            if (pages[pfn - num_pages].state != page_state::free)
                return;

            if (pages[pfn - num_pages].order != order)
                return;

            first_pfn = pfn - num_pages;
            second_pfn = pfn;
        } else { // compact right
            if (pfn + num_pages >= total_pages)
                return;

            if (pages[pfn + num_pages].state != page_state::free)
                return;

            if (pages[pfn + num_pages].order != order)
                return;

            first_pfn = pfn;
            second_pfn = pfn + num_pages;
        }

        remove_entry_list(pages[first_pfn].free.le);
        remove_entry_list(pages[second_pfn].free.le);

        free_folios[order] -= 2;

        for (uint64_t i = 0; i < num_pages * 2; i++) {
            pages[first_pfn + i].order++;
        }

        pages[second_pfn].state = page_state::folio_following;

        insert_tail_list(folio_free_lists[order + 1], pages[first_pfn].free.le);

        free_folios[order + 1]++;

        pfn = first_pfn;
    }
}

void free_folio_no_lock(uint64_t pa) {
    auto pfn = pa >> PAGE_SHIFT;

    if (pages[pfn].state == page_state::free)
        bsod_format("free_folio called for PFN {:x} that is already free", pfn);

    if (pages[pfn].state == page_state::folio_following)
        bsod_format("free_folio called for PFN {:x} with state folio_following", pfn);

    if (pages[pfn].state == page_state::used) {
        pages[pfn].used.refcount--;

        if (pages[pfn].used.refcount != 0)
            return;
    }

#ifdef DEBUG
    if (pfn == zero_pfn)
        bsod("trying to free zero page");
#endif

    insert_tail_list(folio_free_lists[pages[pfn].order], pages[pfn].free.le);
    pages[pfn].state = page_state::free;

    free_folios[pages[pfn].order]++;

    try_to_compact(pfn);
}

export void free_folio(uint64_t pa) {
    lock_guard lock(pa_spinlock);

    free_folio_no_lock(pa);
}

export uint64_t cow_folio(uint64_t pa) {
    uint64_t pfn = pa >> PAGE_SHIFT;
    auto& p = pages[pfn];

    // FIXME - have a list of zeroed pages ready to go, and use these if COWing the zero page

    {
        lock_guard lock(pa_spinlock);

        assert(p.state == page_state::used);

        if (p.used.refcount == 1) // no COW necessary
            return pa;
    }

    auto new_pa = allocate_folio(p.order);
    if (!new_pa)
        return 0;

    memcpy(phys_address(new_pa), phys_address(pa), PAGE_SIZE << p.order);

    bool beaten_to_it;

    {
        lock_guard lock(pa_spinlock);

        if (p.used.refcount == 1)
            beaten_to_it = true;
        else {
            beaten_to_it = false;
            p.used.refcount--;
        }
    }

    if (beaten_to_it) {
        free_folio(new_pa);
        return pa;
    }

    return new_pa;
}

#ifdef __aarch64__
export void unmap_and_free_page(uint64_t va) {
    auto ttbr1 = ttbr1_el1_load();
    auto& level1 = *(ARM64_HARDWARE_PTE*)phys_address(ttbr1 | (((va & 0xff8000000000) >> 39) << 3));
    auto& level2 = *(ARM64_HARDWARE_PTE*)phys_address((level1.pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));
    auto& level3 = *(ARM64_HARDWARE_PTE*)phys_address((level2.pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));
    auto& pt_entry = *(ARM64_HARDWARE_PTE*)phys_address((level3.pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> PAGE_SHIFT) << 3));

    if (!pt_entry.valid)
        return;

    auto pa = ((uint64_t)pt_entry.pfn) << PAGE_SHIFT;

    free_folio(pa);

    pt_entry.valid = 0;
}
#else
export void unmap_and_free_page(uint64_t va) {
    auto& level1 = *(HARDWARE_PTE*)phys_address((__readcr3() & 0xfffffffffffff000) | (((va & 0xff8000000000) >> 39) << 3));
    auto& level2 = *(HARDWARE_PTE*)phys_address((level1.pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));
    auto& level3 = *(HARDWARE_PTE*)phys_address((level2.pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));
    auto& pt_entry = *(HARDWARE_PTE*)phys_address((level3.pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> PAGE_SHIFT) << 3));

    if (!pt_entry.p)
        return;

    auto pa = ((uint64_t)pt_entry.pfn) << PAGE_SHIFT;

    free_folio(pa);

    pt_entry.p = 0;
}
#endif

#ifdef __aarch64__
export void unmap_page(uint64_t va) {
    auto ttbr1 = ttbr1_el1_load();
    auto& level1 = *(ARM64_HARDWARE_PTE*)phys_address(ttbr1 | (((va & 0xff8000000000) >> 39) << 3));
    auto& level2 = *(ARM64_HARDWARE_PTE*)phys_address((level1.pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));
    auto& level3 = *(ARM64_HARDWARE_PTE*)phys_address((level2.pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));
    auto& pt_entry = *(ARM64_HARDWARE_PTE*)phys_address((level3.pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> PAGE_SHIFT) << 3));

    pt_entry.valid = 0;

    __asm __volatile (
        "tlbi vae1, %x[res]\n"
        "dsb ish\n"
        "isb\n"
        :
        : [res] "r" (va >> PAGE_SHIFT)
    );
}
#else
export void unmap_page(uint64_t va) {
    auto& level1 = *(HARDWARE_PTE*)phys_address((__readcr3() & 0xfffffffffffff000) | (((va & 0xff8000000000) >> 39) << 3));
    auto& level2 = *(HARDWARE_PTE*)phys_address((level1.pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));
    auto& level3 = *(HARDWARE_PTE*)phys_address((level2.pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));
    auto& pt_entry = *(HARDWARE_PTE*)phys_address((level3.pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> PAGE_SHIFT) << 3));

    pt_entry.p = 0;

    __asm __volatile(
        "invlpg %0"
        :
        : "m" (va)
    );
}
#endif

#ifdef __aarch64__
export ARM64_HARDWARE_PTE* find_pte(uint64_t va) {
    bool lower_half = !(va & 0x8000000000000000);

    // FIXME - huge pages

    auto pte0 = span((ARM64_HARDWARE_PTE*)phys_address(lower_half ? ttbr0_el1_load() : ttbr1_el1_load()),
                     PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p0 = pte0[(va & 0xff8000000000) >> 39];

    if (!p0.valid)
        return nullptr;

    auto pte1 = span((ARM64_HARDWARE_PTE*)phys_address(p0.pfn << PAGE_SHIFT),
                     PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p1 = pte1[(va & 0x7fc0000000) >> 30];

    if (!p1.valid)
        return nullptr;

    auto pte2 = span((ARM64_HARDWARE_PTE*)phys_address(p1.pfn << PAGE_SHIFT),
                     PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto& p2 = pte2[(va & 0x3fe00000) >> 21];

    if (!p2.valid)
        return nullptr;

    auto pte3 = span((ARM64_HARDWARE_PTE*)phys_address(p2.pfn << PAGE_SHIFT),
                     PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

    return &pte3[(va & 0x1ff000) >> PAGE_SHIFT];
}

export void protect_page(uint64_t va, enum mapping_type type) {
    // FIXME - can we do this without calling ttbr1_el1_load every time?

    auto& pte = *find_pte(va);

    pte.UXN = 1;

    switch (type) {
        case mapping_type::read_write:
            pte.AP = ARM64_PTE_AP_EL1RW;
            pte.PXN = 1;
        break;

        case mapping_type::read_only:
            pte.AP = ARM64_PTE_AP_EL1RO;
            pte.PXN = 1;
        break;

        case mapping_type::read_execute:
            pte.AP = ARM64_PTE_AP_EL1RO;
            pte.PXN = 0;
        break;

        case mapping_type::read_write_execute:
            pte.AP = ARM64_PTE_AP_EL1RW;
            pte.PXN = 0;
        break;

        case mapping_type::no_cache:
        case mapping_type::no_access:
        case mapping_type::write_combine:
        case mapping_type::reserved:
            break;
    }
}
#else
static void protect_page2(volatile HARDWARE_PTE& pt_entry, enum mapping_type type, bool ks) {
    lock_guard lock(protect_page_spinlock);

    // FIXME - do we really need the spinlock?

    switch (type) {
        case mapping_type::read_write:
            pt_entry.xd = 1;
            pt_entry.w = 1;
            pt_entry.ps = 0;
            pt_entry.pcd = 0;
            pt_entry.pwt = 0;
        break;

        case mapping_type::read_only:
            pt_entry.xd = 1;
            pt_entry.w = 0;
            pt_entry.ps = 0;
            pt_entry.pcd = 0;
            pt_entry.pwt = 0;
        break;

        case mapping_type::no_access:
        case mapping_type::read_execute:
            pt_entry.xd = 0;
            pt_entry.w = 0;
            pt_entry.ps = 0;
            pt_entry.pcd = 0;
            pt_entry.pwt = 0;
        break;

        case mapping_type::read_write_execute:
            pt_entry.xd = 0;
            pt_entry.w = 1;
            pt_entry.ps = 0;
            pt_entry.pcd = 0;
            pt_entry.pwt = 0;
        break;

        case mapping_type::no_cache:
            pt_entry.xd = 1;
            pt_entry.w = 1;
            pt_entry.ps = 0;
            pt_entry.pcd = 1;
            pt_entry.pwt = 0;
        break;

        case mapping_type::write_combine:
            pt_entry.xd = 1;
            pt_entry.w = 1;
            pt_entry.ps = 1;
            pt_entry.pcd = 1;
            pt_entry.pwt = 0;
        break;

        case mapping_type::reserved:
        break;
    }

    pt_entry.u = (type == mapping_type::no_access || ks) ? 0 : 1;
}

export void protect_page(uint64_t va, enum mapping_type type) {
    volatile auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    protect_page2(pt_entry, type, va & 0x8000000000000000);
}
#endif

static void mark_vamem_as_used(uint64_t off, size_t num_pages) {
    if (off & 63) {
        auto& b = vablock[off >> 6];

        while (off & 63 && num_pages != 0) {
            b &= ~((uint64_t)1 << (63 - (off & 63)));
            off++;
            num_pages--;
        }
    }

    if (num_pages == 0)
        return;

    memset(vablock + (off >> 6), 0, (num_pages >> 6) * sizeof(uint64_t));

    if (num_pages & 63) {
        off += num_pages & (uint64_t)~63;
        num_pages &= 63;

        auto& b = vablock[off >> 6];

        while (num_pages != 0) {
            b &= ~((uint64_t)1 << (63 - (off & 63)));
            off++;
            num_pages--;
        }
    }
}

export bool init_vablock() {
    vablock_size = bytes_to_pages((total_pages + 7) / 8) * 2 << PAGE_SHIFT;

    // VA block reserved is 64 GB, don't go over this
    if (vablock_size > 0x200000)
        vablock_size = 0x200000;

    auto num_pages = vablock_size >> PAGE_SHIFT;
    auto va = (uintptr_t)vablock;

    for (unsigned int i = 0; i < num_pages; i++) {
        auto pa = allocate_folio_no_lock(0, page_state::used);
        if (!pa) {
            print("allocate_folio_no_lock failed\n");
            return false;
        }

#ifdef __aarch64__
        if (!map_page<true>(pa, va, mapping_type::read_write)) {
            print("map_page failed\n");
            return false;
        }
#else
        if (!map_page(pa, va, mapping_type::read_write)) {
            print("map_page failed\n");
            return false;
        }
#endif

        va += PAGE_SIZE;
    }

    // 0 = used, 1 = free
    memset(vablock, 0xff, vablock_size);

    mark_vamem_as_used(0, num_pages);

    return true;
}

static uint64_t leading_ones(uint64_t n) {
    uint64_t ret = 0;

    while (n & 0x8000000000000000) {
        ret++;
        n <<= 1;
    }

    return ret;
}

static uint64_t trailing_ones(uint64_t n) {
    uint64_t ret = 0;

    while (n & 1) {
        ret++;
        n >>= 1;
    }

    return ret;
}

export uint64_t allocate_va_no_lock(uint32_t num_pages) {
    auto mb = span((uint64_t*)vablock, vablock_size / sizeof(uint64_t));
    uint64_t run_start = 0;
    uint64_t run_length = 0;
    uint64_t mask;

    if (num_pages < 64)
        mask = (((uint64_t)1 << num_pages) - 1) << (uint64_t)(64 - num_pages);

    // start from where we left off last time, to reduce TLB caching issues

    static uint64_t last_i = 0, last_j = 0;

    for (uint64_t i = last_i; i < mb.size(); i++) {
        if (mb[i] & 0x8000000000000000 && last_j == 0) {
            if (run_length == 0)
                run_start = i * 64;

            run_length += leading_ones(mb[i]);

            if (run_length >= num_pages) {
                mark_vamem_as_used(run_start, num_pages);

                uint64_t run_end = run_start + num_pages;

                last_i = run_end / 64;
                last_j = run_end % 64;

                return (uintptr_t)vablock + (run_start << PAGE_SHIFT);
            }
        } else
            run_length = 0;

        if (num_pages < 64) {
            auto m = mask;
            for (uint64_t j = 0; j < 64 - num_pages; j++) {
                if (j >= last_j && (mb[i] & m) == m) {
                    mb[i] &= ~m;
                    last_i = i;
                    last_j = j + num_pages;
                    return (uintptr_t)vablock + (((i * 64) + j) << PAGE_SHIFT);
                }

                m >>= 1;
            }
        }

        auto val = mb[i];

        if (i == last_i && last_j != 0) { // clear first last_j bits
            auto mask = (uint64_t)1 << last_j;

            mask--;
            mask <<= 64 - last_j;
            val &= ~mask;
        }

        last_j = 0;

        if (val == 0xffffffffffffffff)
            continue;

        auto t1 = trailing_ones(val);
        run_length = t1;
        run_start = ((i + 1) * 64) - t1;
    }

    // FIXME - if we reach the end, issue IPIs to clear TLBs on other CPUs, and start again from beginning

    if (run_length >= num_pages) {
        mark_vamem_as_used(run_start, num_pages);
        return (uintptr_t)vablock + (run_start << PAGE_SHIFT);
    }

    return 0;
}

export uint64_t allocate_va(uint32_t num_pages) {
    lock_guard lock(va_spinlock);

    return allocate_va_no_lock(num_pages);
}

export void free_va(uint64_t va, uint32_t num_pages) {
    va -= (uintptr_t)vablock;
    va >>= PAGE_SHIFT;

    lock_guard lock(va_spinlock);

    auto mb = span((uint64_t*)vablock, vablock_size / sizeof(uint64_t));

    for (unsigned int n = 0; n < num_pages; n++) {
        uint64_t i = va / 64;
        uint64_t j = 63 - (va % 64);

        mb[i] |= (uint64_t)1 << j;

        va++;
    }
}

#ifdef __aarch64__
export uint64_t va_to_pa_lookup(uint64_t va) {
    auto ttbr1 = ttbr1_el1_load();
    auto& level1 = *(ARM64_HARDWARE_PTE*)phys_address(ttbr1 | (((va & 0xff8000000000) >> 39) << 3));
    auto& level2 = *(ARM64_HARDWARE_PTE*)phys_address((level1.pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));
    auto& level3 = *(ARM64_HARDWARE_PTE*)phys_address((level2.pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));
    auto& pt_entry = *(ARM64_HARDWARE_PTE*)phys_address((level3.pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> PAGE_SHIFT) << 3));

    return (uint64_t)pt_entry.pfn << PAGE_SHIFT;
}
#else
export uint64_t va_to_pa_lookup(uint64_t va) {
    auto& level1 = *(HARDWARE_PTE*)phys_address((__readcr3() & 0xfffffffffffff000) | (((va & 0xff8000000000) >> 39) << 3));
    auto& level2 = *(HARDWARE_PTE*)phys_address((level1.pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));
    auto& level3 = *(HARDWARE_PTE*)phys_address((level2.pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));
    auto& pt_entry = *(HARDWARE_PTE*)phys_address((level3.pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> PAGE_SHIFT) << 3));

    return (uint64_t)pt_entry.pfn << PAGE_SHIFT;
}
#endif

export
extern "C"
void* memchr(const void* ptr, int ch, size_t count) {
    auto p = (uint8_t*)ptr;

    for (size_t i = 0; i < count; i++) {
        if (*p == ch)
            return p;

        p++;
    }

    return nullptr;
}

export
extern "C"
int strcmp(const char* s1, const char* s2) {
    size_t i = 0;

    while (true) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 == 0 && c2 == 0)
            return 0;
        else if (c1 == 0)
            return -1;
        else if (c2 == 0)
            return 1;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        i++;
    }
}

export
extern "C"
char* strcpy(char* dest, const char* src) {
    char* orig_dest = dest;

    while (*src != 0) {
        *dest = *src;
        src++;
        dest++;
    }

    *dest = 0;

    return orig_dest;
}

export
extern "C"
void* memmove(void* dest, const void* src, size_t n) {
    auto orig_dest = dest;

    if (dest <= src) {
        while (n > 0) {
            *(uint8_t*)dest = *(uint8_t*)src;

            dest = (uint8_t*)dest + 1;
            src = (uint8_t*)src + 1;

            n--;
        }
    } else {
        dest = (uint8_t*)dest + n - 1;
        src = (uint8_t*)src + n - 1;

        while (n > 0) {
            *(uint8_t*)dest = *(uint8_t*)src;

            dest = (uint8_t*)dest - 1;
            src = (uint8_t*)src - 1;

            n--;
        }
    }

    return orig_dest;
}

export
extern "C"
int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 == 0 && c2 == 0)
            return 0;
        else if (c1 == 0)
            return -1;
        else if (c2 == 0)
            return 1;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;
    }

    return 0;
}

template<unsigned N, enum page_state S>
template<bool no_lock>
void* allocator_folio<N, S>::malloc() {
    uint64_t p;

    if constexpr (no_lock)
        p = allocate_folio_no_lock(N, S);
    else
        p = allocate_folio(N, S);

    if (!p)
        return nullptr;

    return phys_address(p);
}

template<unsigned N, enum page_state S>
template<bool no_lock>
void allocator_folio<N, S>::free(void* ptr) {
    auto pfn = phys_memory_pfn(ptr);

    if constexpr (no_lock)
        free_folio_no_lock(pfn << PAGE_SHIFT);
    else
        free_folio(pfn << PAGE_SHIFT);
}

template<unsigned N, unsigned P, enum page_state S>
template<bool no_lock>
uint64_t allocator_cb<N, P, S>::malloc_pa() {
    static constexpr unsigned int BITMAP_BITS_PER_PAGE = sizeof(uint64_t) * 8 / PAGES_PER_BITMAP_BLOCK;
    static constexpr size_t GRANULARITY = PAGE_SIZE / BITMAP_BITS_PER_PAGE;
    static constexpr uint64_t MASK = BITMAP_BITS_PER_PAGE == 64 ? 0xffffffffffffffff : ((uint64_t)1 << BITMAP_BITS_PER_PAGE) - 1;

    for (unsigned int i = 0; i < NUM_PAGES / PAGES_PER_BITMAP_BLOCK; i++) {
        unsigned int bit;

        if (bitmap[i] == 0xffffffffffffffff || bitmap[i] == 0)
            continue;

        auto b2 = bitmap[i];
        bool found = false;

        bit = 0;

        for (unsigned j = 0; j < PAGES_PER_BITMAP_BLOCK; j++) {
            if ((b2 & MASK) != MASK && (b2 & MASK) != 0) {
                for (unsigned int k = 0; k < BITMAP_BITS_PER_PAGE; k++) {
                    if (!(b2 & 0x1)) {
                        found = true;
                        break;
                    }

                    bit++;
                    b2 >>= 1;
                }

                if (found)
                    break;
            } else {
                if constexpr (BITMAP_BITS_PER_PAGE < 64)
                    b2 >>= BITMAP_BITS_PER_PAGE;

                bit += BITMAP_BITS_PER_PAGE;
            }
        }

        if (!found)
            continue;

        assert(!(bitmap[i] & ((uint64_t)1 << bit)));
        bitmap[i] |= (uint64_t)1 << bit;

        unsigned int index = (i * PAGES_PER_BITMAP_BLOCK) + PAGES_PER_BITMAP_BLOCK - 1;

        for (unsigned int j = 0; j < PAGES_PER_BITMAP_BLOCK; j++) {
            if (bit < BITMAP_BITS_PER_PAGE) {
                uint64_t addr = (pfns[index] << PAGE_SHIFT) + ((BITMAP_BITS_PER_PAGE - 1) * GRANULARITY);

                for (unsigned int k = 0; k < BITMAP_BITS_PER_PAGE; k++) {
                    if (bit == 0)
                        return addr;

                    bit--;
                    addr -= GRANULARITY;
                }
            } else {
                index--;
                bit -= BITMAP_BITS_PER_PAGE;
            }
        }
    }

    uint64_t pfn;

    if constexpr (no_lock)
        pfn = allocate_folio_no_lock(0, page_state_type) >> PAGE_SHIFT;
    else
        pfn = allocate_page(page_state_type) >> PAGE_SHIFT;

    if (!pfn)
        return 0;

    auto& p = pages[pfn];

    p.allocator.cb = this;

    bool found = false;

    for (unsigned int i = 0; i < NUM_PAGES / PAGES_PER_BITMAP_BLOCK; i++) {
        unsigned int bit;

        if (bitmap[i] == 0xffffffffffffffff)
            continue;

        auto b2 = bitmap[i];

        for (unsigned int j = 0; j < PAGES_PER_BITMAP_BLOCK; j++) {
            if (!(b2 & MASK)) {
                bit = (j * BITMAP_BITS_PER_PAGE) + BITMAP_BITS_PER_PAGE - 1;
                p.allocator.index = (i * PAGES_PER_BITMAP_BLOCK) + PAGES_PER_BITMAP_BLOCK - 1 - j;
                found = true;
                break;
            }

            if constexpr (BITMAP_BITS_PER_PAGE < 64)
                b2 >>= BITMAP_BITS_PER_PAGE;
        }

        if (!found)
            continue;

        assert(!(bitmap[i] & (uint64_t)1 << bit));

        bitmap[i] |= (uint64_t)1 << bit;
        pfns[p.allocator.index] = pfn;

        return pfn << PAGE_SHIFT;
    }

    free_folio(pfn << PAGE_SHIFT);

    return 0;
}

template<unsigned N, unsigned P, enum page_state S>
template<bool no_lock>
void allocator_cb<N, P, S>::free(uint64_t index) {
    auto& b = bitmap[index / 64];
    unsigned int bit = 63 - (index & 63);

    assert(b & ((uint64_t)1 << bit));

    b &= ~((uint64_t)1 << bit);

    static constexpr unsigned int BITMAP_BITS_PER_PAGE = sizeof(uint64_t) * 8 / PAGES_PER_BITMAP_BLOCK;
    static constexpr uint64_t MASK = BITMAP_BITS_PER_PAGE == 64 ? 0xffffffffffffffff : ((uint64_t)1 << BITMAP_BITS_PER_PAGE) - 1;

    auto b2 = b;

    for (unsigned int i = 0; i < sizeof(uint64_t) * 8 / BITMAP_BITS_PER_PAGE; i++) {
        if ((bit < BITMAP_BITS_PER_PAGE) && b2 & MASK)
            return;

        bit -= BITMAP_BITS_PER_PAGE;

        if constexpr (BITMAP_BITS_PER_PAGE < 64)
            b2 >>= BITMAP_BITS_PER_PAGE;
    }

    if constexpr (no_lock)
        free_folio_no_lock(pfns[index / BITMAP_BITS_PER_PAGE] << PAGE_SHIFT);
    else
        free_folio(pfns[index / BITMAP_BITS_PER_PAGE] << PAGE_SHIFT);

#ifdef DEBUG
    pfns[index / BITMAP_BITS_PER_PAGE] = 0;
#endif
}

template<unsigned N, unsigned P, enum page_state S>
template<bool no_lock>
void* ::allocator<N, P, S>::malloc() {
    // FIXME - move calls to allocate_page out of this object's spinlock

    auto func = [&]() -> void* {
        for (auto le = control_blocks.next; le != &control_blocks; le = le->next) {
            auto& cb = containing_record<cb_type>(le);

            auto pa = cb.template malloc_pa<no_lock>();
            if (pa)
                return phys_address(pa);
        }

        uint64_t p;

        if constexpr (no_lock)
            p = allocate_folio_no_lock(0, page_state::used);
        else
            p = allocate_page();

        auto cb = (cb_type*)phys_address(p);

        new (cb) cb_type;

        insert_tail_list(control_blocks, cb->le);

        auto pa = cb->template malloc_pa<no_lock>();
        if (!pa)
            return nullptr;

        return phys_address(pa);
    };

    if constexpr (no_lock)
        return func();
    else {
        lock_guard l(lock);
        return func();
    }
}

template<unsigned N, unsigned P, enum page_state S>
template<bool no_lock>
void ::allocator<N, P, S>::free(void* ptr) {
    auto pfn = phys_memory_pfn(ptr);
    auto& p = pages[pfn];

    auto& cb = *(cb_type*)p.allocator.cb;

    auto off = (uintptr_t)ptr & (PAGE_SIZE - 1);

    static constexpr size_t GRANULARITY = PAGE_SIZE * cb_type::PAGES_PER_BITMAP_BLOCK / (8 * sizeof(uint64_t));

    assert(!(off & (GRANULARITY - 1)));
    assert(cb.pfns[p.allocator.index] == pfn);

    uint64_t index = p.allocator.index * 8 * sizeof(uint64_t) / cb_type::PAGES_PER_BITMAP_BLOCK;

    while (off > 0) {
        off -= GRANULARITY;
        index++;
    }

    if constexpr (no_lock)
        cb.template free<true>(index);
    else {
        lock_guard l(lock);
        cb.template free<false>(index);
    }
}

export void* vmalloc(size_t size) {
    uint64_t num_pages = bytes_to_pages(size);

    auto va = allocate_va((uint32_t)num_pages);
    if (!va) {
        print("allocate_va failed\n");
        return nullptr;
    }

    for (size_t i = 0; i < num_pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");

            for (size_t j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va + (j << PAGE_SHIFT));

                free_folio(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, (uint32_t)num_pages);

            return nullptr;
        }

        if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
            free_folio(pa);

            for (size_t j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va + (j << PAGE_SHIFT));

                free_folio(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, (uint32_t)num_pages);

            return nullptr;
        }
    }

    return (void*)va;
}

export void vfree(void* ptr, size_t size) {
    uint64_t num_pages = bytes_to_pages(size);

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_and_free_page((uint64_t)ptr + (i << PAGE_SHIFT));
    }
}

export void inline __attribute__((always_inline)) barrier() {
#ifdef __aarch64__
    __asm __volatile (
        "dsb ish\n\t"
        "isb\n\t"
    );
#else
    __asm__ __volatile__ (
        ""
        :
        :
        : "memory"
    );
#endif
}

export constexpr uint8_t bytes_to_folio_order(size_t s) {
    uint32_t p = bytes_to_pages(s);

    if (p == 0)
        return 0;

    return (uint8_t)(32 - countl_zero(p - 1));
}

export void increase_page_refcount(uint64_t pfn) {
    auto& p = pages[pfn];

    // FIXME - can we avoid global spinlock?

    {
        lock_guard lock(pa_spinlock);

        assert(p.state == page_state::used);

        p.used.refcount++;
    }
}

export void* allocate_stack() {
    auto stack = (void*)allocate_va(KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    if (!stack) {
        print("allocate_va failed\n");
        return nullptr;
    }

    for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto va = (uintptr_t)stack + ((j + KERNEL_GUARD_PAGES) << PAGE_SHIFT);
                auto pa = va_to_pa_lookup(va);

                free_folio(pa);
                unmap_page(va);
            }

            free_va((uintptr_t)stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);

            return nullptr;
        }

        if (!map_page(pa, (uintptr_t)stack + ((i + KERNEL_GUARD_PAGES) << PAGE_SHIFT), mapping_type::read_write)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto va = (uintptr_t)stack + ((j + KERNEL_GUARD_PAGES) << PAGE_SHIFT);
                auto pa = va_to_pa_lookup(va);

                free_folio(pa);
                unmap_page(va);
            }

            free_va((uintptr_t)stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);

            return nullptr;
        }
    }

    return stack;
}

export void* allocate_stack_no_lock() {
    auto stack = (void*)allocate_va_no_lock(KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);
    if (!stack) {
        print("allocate_va failed\n");
        return nullptr;
    }

    for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
        auto pa = allocate_folio_no_lock(0, page_state::used);
        if (!pa) {
            print("allocate_folio_no_lock failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto va = (uintptr_t)stack + ((j + KERNEL_GUARD_PAGES) << PAGE_SHIFT);
                auto pa = va_to_pa_lookup(va);

                free_folio(pa);
                unmap_page(va);
            }

            free_va((uintptr_t)stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);

            return nullptr;
        }

        if (!map_page<true>(pa, (uintptr_t)stack + ((i + KERNEL_GUARD_PAGES) << PAGE_SHIFT), mapping_type::read_write)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto va = (uintptr_t)stack + ((j + KERNEL_GUARD_PAGES) << PAGE_SHIFT);
                auto pa = va_to_pa_lookup(va);

                free_folio(pa);
                unmap_page(va);
            }

            free_va((uintptr_t)stack, KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES);

            return nullptr;
        }
    }

    return stack;
}

export enum mapping_type characteristics_to_mapping_type(uint32_t c) {
    if (c & IMAGE_SCN_MEM_WRITE && !(c & IMAGE_SCN_MEM_EXECUTE))
        return mapping_type::read_write;
    else if (c & IMAGE_SCN_MEM_WRITE && c & IMAGE_SCN_MEM_EXECUTE)
        return mapping_type::read_write_execute;
    else if (!(c & IMAGE_SCN_MEM_WRITE) && !(c & IMAGE_SCN_MEM_EXECUTE))
        return mapping_type::read_only;
    else if (!(c & IMAGE_SCN_MEM_WRITE) && c & IMAGE_SCN_MEM_EXECUTE)
        return mapping_type::read_execute;

    return mapping_type::read_only;
}

export void protect_kernel_pages() {
    const auto& dos_header = *(IMAGE_DOS_HEADER*)&__image_base__;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
        print("Incorrect DOS signature.\n");
        return;
    }

    const auto& nt_header = *(IMAGE_NT_HEADERS*)(&__image_base__ + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE) {
        print("Incorrect PE signature.\n");
        return;
    }

    span<const IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else {
        print("Unrecognized optional header signature.\n");
        return;
    }

    // first page
    protect_page((uintptr_t)&__image_base__, mapping_type::read_only);

    for (const auto& sect : sections) {
        if (sect.VirtualSize == 0)
            continue;

        auto pages = bytes_to_pages(sect.VirtualSize);
        auto va = (uintptr_t)&__image_base__ + sect.VirtualAddress;

        for (unsigned int i = 0; i < pages; i++) {
            protect_page(va, characteristics_to_mapping_type(sect.Characteristics));
            va += PAGE_SIZE;
        }
    }
}

static bool map_uefi(const loader_block& lb) {
    auto map_entries = span((map_entry*)lb.map_ptr, lb.num_map_entries);
    unsigned int count = 0;

    // FIXME
    using EFI_MEMORY_DESCRIPTOR = _EFI_MEMORY_DESCRIPTOR;
    using EFI_MEMORY_TYPE = _EFI_MEMORY_TYPE;

    uint64_t va = UEFI_VABLOCK;

    // FIXME - bsod if we would overflow uefi_mappings

    auto d = uefi_mappings;
    for (const auto& m : map_entries) {
        if (m.type == mem_type::uefi_code)
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesCode;
        else if (m.type == mem_type::uefi_data)
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesData;
        else
            continue;

        for (unsigned int i = 0; i < m.len >> PAGE_SHIFT; i++) {
            if (!map_page<true>(m.addr + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT),
                                mapping_type::read_write_execute)) { // FIXME
                print("map_page failed\n");
                return false;
            }
        }

        d->Pad = 0;
        d->PhysicalStart = m.addr;
        d->VirtualStart = va;
        d->NumberOfPages = m.len >> PAGE_SHIFT;
        d->Attribute = 0;

        va += m.len;
        count++;
    }

    if (count == 0)
        bsod("SetVirtualAddressMap set but no UEFI mappings given");

    auto ret = lb.SetVirtualAddressMap(count * sizeof(EFI_MEMORY_DESCRIPTOR),
                                       sizeof(EFI_MEMORY_DESCRIPTOR),
                                       EFI_MEMORY_DESCRIPTOR_VERSION,
                                       uefi_mappings);
    if (ret != 0) { // FIXME - enum
        print("SetVirtualAddressMap returned {}\n", ret);
        return false;
    }

    // FIXME - rebase GetTime

    return true;
}
