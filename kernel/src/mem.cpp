module;

#include <loaderblock.h>
#include "kernel.h"
#include "acpica.h"
#include "mem.h"
#include "spinlock.h"
#include <span>
#include <cstring>
#include <bit>

export module mem;

import print;
import bsod;
import config;
import list;

// mem.S
export {
    extern "C" void memcpy_aligned(void* dest, const void* src, size_t n);
    extern "C" void init_pat();
    extern "C" void invalidate_tlb_isr();
}

using namespace std;

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
            void* cb;
            unsigned int index;
        } allocator;
    };
};

static_assert(sizeof(struct page) == 24);

page* pages = (page*)0xfffffa0000000000;
static uint64_t total_pages;
static spinlock pa_spinlock;

static uint64_t* const vablock = (uint64_t*)0xfffffa4000000000;
static uint64_t vablock_size;

static uint8_t* const initrd_addr = (uint8_t*)0xfffffa5000000000;

static spinlock va_spinlock, heap_spinlock, tlb_spinlock;
static spinlock mapping_spinlock;
static spinlock protect_page_spinlock;

export span<const uint8_t> initrd;
extern "C" uint64_t tlb_va;
extern "C" volatile uint32_t invalidate_tlb_processors_left;

constexpr uint64_t PHYS_MEMORY_MASK = 0xffffff0000000000;
static const uint64_t PHYS_MEMORY = 0xffff990000000000;

export constexpr uint8_t MAX_FOLIO_ORDER = 9; // 2MB

static list_entry folio_free_lists[MAX_FOLIO_ORDER + 1];
static uint64_t free_folios[MAX_FOLIO_ORDER + 1];

static uint8_t MAXPHYADDR = 36;

template<unsigned N, enum page_state S>
class allocator_folio {
public:
    void* malloc();
    void free(void* ptr);
};

template<unsigned N, unsigned P, enum page_state S>
struct allocator_cb {
    allocator_cb() {
        memset(bitmap, 0, sizeof(bitmap));
    }

    uint64_t malloc_pa();
    void free(uint64_t index);

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
    void* malloc();
    void free(void* ptr);

    using cb_type = allocator_cb<N, P, S>;

    spinlock lock;
    list_entry control_blocks = { &control_blocks, &control_blocks };
};

static allocator_folio<1, page_state::allocator_8k> alloc8k;
static allocator_folio<0, page_state::allocator_4k> alloc4k;
static ::allocator<488, 32, page_state::allocator_2k> alloc2k;
static ::allocator<480, 16, page_state::allocator_1k> alloc1k;
static ::allocator<448, 8, page_state::allocator_512> alloc512;
static ::allocator<408, 4, page_state::allocator_256> alloc256;
static ::allocator<336, 2, page_state::allocator_128> alloc128;
static ::allocator<255, 1, page_state::allocator_64> alloc64;
// FIXME - alloc32, alloc16, alloc8?

static_assert(sizeof(decltype(alloc2k)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc1k)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc512)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc256)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc128)::cb_type) <= PAGE_SIZE);
static_assert(sizeof(decltype(alloc64)::cb_type) <= PAGE_SIZE);

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
        *(uint32_t*)s = v;

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

static bool map_page_with_scratch(uint64_t va, uint64_t pa, uint64_t& scratch_pa, uint64_t& scratch_pages) {
    auto pml4 = span((HARDWARE_PTE*)SELFMAP_PML4, 512);
    auto pdpt = span((HARDWARE_PTE*)(SELFMAP_PDPT | (((va & 0xff8000000000) >> 39) << PAGE_SHIFT)), 512);
    auto pdt = span((HARDWARE_PTE*)(SELFMAP_PDT | (((va & 0xffffc0000000) >> 30) << PAGE_SHIFT)), 512);
    auto pt = span((HARDWARE_PTE*)(SELFMAP | (((va & 0xffffffe00000) >> 21) << PAGE_SHIFT)), 512);

    auto& pml4_entry = pml4[(va & 0xff8000000000) >> 39];

    if (!pml4_entry.Valid) {
        if (scratch_pages == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pml4_entry.PageFrameNumber = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pml4_entry.Valid = 1;
        pml4_entry.Write = 1;

        memset(pdpt.data(), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto& pdpt_entry = pdpt[(va & 0x7fc0000000) >> 30];

    if (!pdpt_entry.Valid) {
        if (scratch_pages == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdpt_entry.PageFrameNumber = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pdpt_entry.Valid = 1;
        pdpt_entry.Write = 1;

        memset(pdt.data(), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto& pdt_entry = pdt[(va & 0x3fe00000) >> 21];

    if (!pdt_entry.Valid) {
        if (scratch_pages == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdt_entry.PageFrameNumber = scratch_pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pdt_entry.Valid = 1;
        pdt_entry.Write = 1;

        memset(pt.data(), 0, PAGE_SIZE);

        scratch_pa += PAGE_SIZE;
        scratch_pages--;
    }

    auto& pt_entry = pt[(va & 0x1ff000) >> PAGE_SHIFT];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pt_entry.PageFrameNumber = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pt_entry.Valid = 1;
    pt_entry.Write = 1;
    pt_entry.NoExecute = 1;

    return true;
}

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

export uint64_t allocate_folio(uint8_t order, enum page_state state = page_state::used) {
    lock_guard lock(pa_spinlock);

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

        free_folios[order]--;

        return pfn << PAGE_SHIFT;
    }

    return 0;
}

export uint64_t allocate_page(enum page_state state = page_state::used) {
    return allocate_folio(0, state);
}

__attribute__((always_inline))
static uint64_t inline __readcr3() {
    uint64_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr3"
        : "=r" (ret)
    );

    return ret;
}

export bool map_page(uint64_t pa, uint64_t va, enum mapping_type type) {
    if (pa >= (uint64_t)1 << MAXPHYADDR) // out of range
        return false;

    lock_guard lock(mapping_spinlock);

    auto pml4_entry_ptr = (volatile uint64_t*)phys_address((__readcr3() & 0xfffffffffffff000) | (((va & 0xff8000000000) >> 39) << 3));

    HARDWARE_PTE pml4_entry;

    *(uint64_t*)&pml4_entry = *pml4_entry_ptr;

    if (!pml4_entry.Valid) {
        uint64_t page = allocate_page(); // FIXME - should there be an allocate_zeroed_page?
        if (page == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pml4_entry.PageFrameNumber = page >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pml4_entry.Valid = 1;
        pml4_entry.Write = 1;
        pml4_entry.Owner = va & 0x8000000000000000 ? 0 : 1;

        memset(phys_address(page), 0, PAGE_SIZE);

        *pml4_entry_ptr = *(uint64_t*)&pml4_entry;
    }

    uint64_t pfn = pml4_entry.PageFrameNumber;
    auto pdpt_entry_ptr = (volatile uint64_t*)phys_address((pfn << PAGE_SHIFT) | (((va & 0x7fc0000000) >> 30) << 3));

    HARDWARE_PTE pdpt_entry;

    *(uint64_t*)&pdpt_entry = *pdpt_entry_ptr;

    if (!pdpt_entry.Valid) {
        uint64_t page = allocate_page();
        if (page == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdpt_entry.PageFrameNumber = page >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pdpt_entry.Valid = 1;
        pdpt_entry.Write = 1;
        pdpt_entry.Owner = va & 0x8000000000000000 ? 0 : 1;

        memset(phys_address(page), 0, PAGE_SIZE);

        *pdpt_entry_ptr = *(uint64_t*)&pdpt_entry;
    }

    pfn = pdpt_entry.PageFrameNumber;
    auto pdt_entry_ptr = (volatile uint64_t*)phys_address((pfn << PAGE_SHIFT) | (((va & 0x3fe00000) >> 21) << 3));

    HARDWARE_PTE pdt_entry;

    *(uint64_t*)&pdt_entry = *pdt_entry_ptr;

    if (!pdt_entry.Valid) {
        uint64_t page = allocate_page();
        if (page == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdt_entry.PageFrameNumber = page >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pdt_entry.Valid = 1;
        pdt_entry.Write = 1;
        pdt_entry.Owner = va & 0x8000000000000000 ? 0 : 1;

        memset(phys_address(page), 0, PAGE_SIZE);

        *pdt_entry_ptr = *(uint64_t*)&pdt_entry;
    }

    pfn = pdt_entry.PageFrameNumber;
    auto pt_entry_ptr = (volatile uint64_t*)phys_address((pfn << PAGE_SHIFT) | (((va & 0x1ff000) >> 12) << 3));

    HARDWARE_PTE pt_entry;

    *(uint64_t*)&pt_entry = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pt_entry.PageFrameNumber = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pt_entry.Valid = 1;

    switch (type) {
        case mapping_type::no_access:
            pt_entry.NoExecute = 1;
        break;

        case mapping_type::read_write:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
        break;

        case mapping_type::read_only:
            pt_entry.NoExecute = 1;
        break;

        case mapping_type::read_execute:
        break;

        case mapping_type::read_write_execute:
            pt_entry.Write = 1;
        break;

        case mapping_type::no_cache:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
            pt_entry.CacheDisable = 1;
        break;

        case mapping_type::write_combine:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
            pt_entry.LargePage = 1;
            pt_entry.CacheDisable = 1;
        break;

        case mapping_type::reserved:
            return false;
    }

    pt_entry.Owner = (type == mapping_type::no_access || va & 0x8000000000000000) ? 0 : 1;

    *pt_entry_ptr = *(uint64_t*)&pt_entry;

    return true;
}

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

static void mark_mem_as_free(uint64_t pa, size_t len) {
    uint64_t pfn = pa >> PAGE_SHIFT;
    uint64_t num_pages = len >> PAGE_SHIFT;

    lock_guard lock(pa_spinlock);

    for (uint64_t i = 0; i < num_pages; i++) {
        if (pages[pfn].state != page_state::free) {
            insert_tail_list(folio_free_lists[0], pages[pfn].free.le);
            pages[pfn].state = page_state::free;
            free_folios[0]++;
        }

        pfn++;
    }
}

static void mark_mem_as_used(uint64_t pa, size_t len) {
    uint64_t pfn = pa >> PAGE_SHIFT;
    uint64_t num_pages = len >> PAGE_SHIFT;

    lock_guard lock(pa_spinlock);

    for (uint64_t i = 0; i < num_pages; i++) {
        if (pages[pfn].state == page_state::free) {
            remove_entry_list(pages[pfn].free.le);
            pages[pfn].state = page_state::used;
            free_folios[0]--;
        }

        pfn++;
    }
}

export void free_lower_half_mappings() {
    auto pml4 = span((HARDWARE_PTE*)SELFMAP_PML4, 512);

    for (unsigned int i = 0; i < 256; i++) {
        auto& p1 = pml4[i];

        if (p1.Valid) {
            auto pdpt = span((HARDWARE_PTE*)(SELFMAP_PDPT | (i << PAGE_SHIFT)), 512);

            for (unsigned int j = 0; j < pdpt.size(); j++) {
                auto& p2 = pdpt[j];

                if (p2.Valid) {
                    auto pdt = span((HARDWARE_PTE*)(SELFMAP_PDT | (i << (PAGE_SHIFT + 9)) | (j << PAGE_SHIFT)), 512);

                    for (unsigned int k = 0; k < pdt.size(); k++) {
                        auto& p3 = pdt[k];

                        if (p3.Valid) {
                            auto pt = span((HARDWARE_PTE*)(SELFMAP | (i << (PAGE_SHIFT + 18)) | (j << (PAGE_SHIFT + 9)) | (k << PAGE_SHIFT)), 512);

                            memset(pt.data(), 0, PAGE_SIZE);

                            mark_mem_as_free((uint64_t)p3.PageFrameNumber << PAGE_SHIFT, PAGE_SIZE);
                        }
                    }

                    mark_mem_as_free((uint64_t)p2.PageFrameNumber << PAGE_SHIFT, PAGE_SIZE);
                }
            }

            mark_mem_as_free((uint64_t)p1.PageFrameNumber << PAGE_SHIFT, PAGE_SIZE);
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
    pml4_entry.PageFrameNumber = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pml4_entry.Valid = 1;
    pml4_entry.Write = 1;

    memset(pdpt.data(), 0, pdpt.size_bytes());

    static const uint64_t PAGES_PER_GIGABYTE = 1024 * 1024 * 1024 / 4096;

    // FIXME - if more than 512GB, will need to allocate second pml4 entry

    uint64_t gigabytes = (num_pages + PAGES_PER_GIGABYTE - 1) / PAGES_PER_GIGABYTE;
    auto* pdpt_entry = &pdpt[(PHYS_MEMORY & 0x7fc0000000) >> 30];

    for (unsigned int i = 0; i < gigabytes; i++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdpt_entry->PageFrameNumber = i * PAGES_PER_GIGABYTE;
#pragma GCC diagnostic pop
        pdpt_entry->Valid = 1;
        pdpt_entry->Write = 1;
        pdpt_entry->LargePage = 1;

        pdpt_entry++;
    }

    return true;
}

export constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static bool load_initrd(void* data, uintptr_t size) {
    auto num_pages = bytes_to_pages(size);

    for (unsigned int i = 0; i < num_pages; i++) {
        auto pa = allocate_page();
        if (!pa)
            bsod("allocate_page failed in load_initrd");

        if (!map_page(pa, (uintptr_t)initrd_addr + (i << PAGE_SHIFT), mapping_type::read_write))
            bsod("map_page failed in load_initrd");
    }

    memcpy(initrd_addr, data, size);

    initrd = span(initrd_addr, size);

    // FIXME - should free this memory once boot is done

    return true;
}

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

export bool init_memory(const loader_block& lb) {
    uint64_t num_memblock_bytes, num_memblock_pages, orig_scratch_pages, scratch_pages, needed_pages;
    unsigned int num_map_entries = lb.num_map_entries;
    uint64_t memblock_pa;
    auto lb_map = (map_entry*)lb.map_ptr;

    for (unsigned int i = 0; i < MAX_FOLIO_ORDER + 1; i++) {
        folio_free_lists[i].next = &folio_free_lists[i];
        folio_free_lists[i].prev = &folio_free_lists[i];
    }

    init_pat();

    MAXPHYADDR = get_maxphyaddr();

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
            mark_mem_as_free(map.addr, map.len);
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

    return true;
}

#if 0
static void invalidate_tlb(void* va) {
    lock_guard lock(tlb_spinlock);

    tlb_va = (uintptr_t)va;

    __asm __volatile(
        "invlpg [%0]"
        :
        : "b" (va)
        : "memory"
    );

    if (init_smp_finished && num_cpus > 1) {
        invalidate_tlb_processors_left = num_cpus - 1;

        // send IPI to other cores

        auto icr1 = (volatile uint32_t*)&local_apic[0x300];

        *icr1 = 0xc0000 | invalidate_tlb_interrupt;

        while (invalidate_tlb_processors_left > 0) { }
    }
}
#endif

export void* malloc_aligned(size_t size, size_t align) {
    if (size == 0)
        return nullptr;

    if (size < align)
        size = align;

    if (size <= 64)
        return alloc64.malloc();
    else if (size <= 128)
        return alloc128.malloc();
    else if (size <= 256)
        return alloc256.malloc();
    else if (size <= 512)
        return alloc512.malloc();
    else if (size <= 1024)
        return alloc1k.malloc();
    else if (size <= 2048)
        return alloc2k.malloc();
    else if (size <= 4096)
        return alloc4k.malloc();
    else if (size <= 8192)
        return alloc8k.malloc();
    else {
        bsod_format("malloc asked to do over-large allocation ({:x} bytes)\n", size);
        return nullptr;
    }
}

extern "C"
void* malloc(size_t size) {
    return malloc_aligned(size, 1);
}

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

extern "C"
void free(void* ptr) {
    if (!ptr)
        return;

    if (is_phys_memory(ptr)) {
        auto& page = pages[phys_memory_pfn(ptr)];

        switch (page.state) {
            case page_state::allocator_8k:
                alloc8k.free(ptr);
                return;

            case page_state::allocator_4k:
                alloc4k.free(ptr);
                return;

            case page_state::allocator_2k:
                alloc2k.free(ptr);
                return;

            case page_state::allocator_1k:
                alloc1k.free(ptr);
                return;

            case page_state::allocator_512:
                alloc512.free(ptr);
                return;

            case page_state::allocator_256:
                alloc256.free(ptr);
                return;

            case page_state::allocator_128:
                alloc128.free(ptr);
                return;

            case page_state::allocator_64:
                alloc64.free(ptr);
                return;

            default:
                bsod_format("free called for unexpected page state {}", (unsigned int)page.state);
        }
    } else
        bsod_format("free called for non-physical address {:x}", (uintptr_t)ptr);
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

export void free_folio(uint64_t pa) {
    auto pfn = pa >>= PAGE_SHIFT;

    lock_guard lock(pa_spinlock);

    if (pages[pfn].state == page_state::free)
        bsod_format("free_folio called for PFN {:x} that is already free", pfn);

    insert_tail_list(folio_free_lists[pages[pfn].order], pages[pfn].free.le);
    pages[pfn].state = page_state::free;

    free_folios[pages[pfn].order]++;

    try_to_compact(pfn);
}

export void unmap_and_free_page(uint64_t va) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    if (!pt_entry.Valid)
        return;

    auto pa = ((uint64_t)pt_entry.PageFrameNumber) << PAGE_SHIFT;

    free_folio(pa);

    pt_entry.Valid = 0;
}

export void unmap_page(uint64_t va) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    pt_entry.Valid = 0;

    __asm __volatile(
        "invlpg %0"
        :
        : "m" (va)
    );
}

export void protect_page2(volatile HARDWARE_PTE& pt_entry, enum mapping_type type, bool ks) {
    lock_guard lock(protect_page_spinlock);

    // FIXME - do we really need the spinlock?

    switch (type) {
        case mapping_type::read_write:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
            pt_entry.LargePage = 0;
            pt_entry.CacheDisable = 0;
            pt_entry.WriteThrough = 0;
        break;

        case mapping_type::read_only:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 0;
            pt_entry.LargePage = 0;
            pt_entry.CacheDisable = 0;
            pt_entry.WriteThrough = 0;
        break;

        case mapping_type::no_access:
        case mapping_type::read_execute:
            pt_entry.NoExecute = 0;
            pt_entry.Write = 0;
            pt_entry.LargePage = 0;
            pt_entry.CacheDisable = 0;
            pt_entry.WriteThrough = 0;
        break;

        case mapping_type::read_write_execute:
            pt_entry.NoExecute = 0;
            pt_entry.Write = 1;
            pt_entry.LargePage = 0;
            pt_entry.CacheDisable = 0;
            pt_entry.WriteThrough = 0;
        break;

        case mapping_type::no_cache:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
            pt_entry.LargePage = 0;
            pt_entry.CacheDisable = 1;
            pt_entry.WriteThrough = 0;
        break;

        case mapping_type::write_combine:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
            pt_entry.LargePage = 1;
            pt_entry.CacheDisable = 1;
            pt_entry.WriteThrough = 0;
        break;

        case mapping_type::reserved:
        break;
    }

    pt_entry.Owner = (type == mapping_type::no_access || ks) ? 0 : 1;
}

export void protect_page(uint64_t va, enum mapping_type type) {
    volatile auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    protect_page2(pt_entry, type, va & 0x8000000000000000);
}

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
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");
            return false;
        }

        if (!map_page(pa, va, mapping_type::read_write)) {
            print("map_page failed\n");
            return false;
        }

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

export uint64_t allocate_va(uint32_t num_pages) {
    auto mb = span((uint64_t*)vablock, vablock_size / sizeof(uint64_t));
    uint64_t run_start = 0;
    uint64_t run_length = 0;
    uint64_t mask;

    if (num_pages < 64)
        mask = (((uint64_t)1 << num_pages) - 1) << (uint64_t)(64 - num_pages);

    lock_guard lock(va_spinlock);

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

export uint64_t va_to_pa_lookup(uint64_t va) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    return (uint64_t)pt_entry.PageFrameNumber << PAGE_SHIFT;
}

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
long int strtol(const char* nptr, char** endptr, int base) {
    long int val;

    while (*nptr == ' ' || *nptr == '\t') {
        nptr++;
    }

    val = 0;

    while (true) {
        if (*nptr >= '0' && *nptr <= '9') {
            val *= base;
            val += *nptr - '0';
        } else {
            if (endptr)
                *endptr = (char*)nptr;

            return val;
        }

        nptr++;
    }
}

export
extern "C"
char* strstr(const char* haystack, const char* needle) {
    size_t len = strlen(needle);

    while (true) {
        bool found = true;

        for (size_t i = 0; i < len; i++) {
            if (haystack[i] == 0)
                return nullptr;

            if (haystack[i] != needle[i]) {
                found = false;
                break;
            }
        }

        if (found)
            return (char*)haystack;

        haystack++;
    }

    return nullptr;
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
void* allocator_folio<N, S>::malloc() {
    auto p = allocate_folio(N, S);

    if (!p)
        return nullptr;

    return phys_address(p);
}

template<unsigned N, enum page_state S>
void allocator_folio<N, S>::free(void* ptr) {
    auto pfn = phys_memory_pfn(ptr);

    free_folio(pfn << PAGE_SHIFT);
}

template<unsigned N, unsigned P, enum page_state S>
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

        assert(!(bitmap[i] & (1 << bit)));
        bitmap[i] |= 1 << bit;

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

    auto pfn = allocate_page(page_state_type) >> PAGE_SHIFT;

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

        assert(!(bitmap[i] & 1 << bit));

        bitmap[i] |= 1 << bit;
        pfns[p.allocator.index] = pfn;

        return pfn << PAGE_SHIFT;
    }

    free_folio(pfn << PAGE_SHIFT);

    return 0;
}

template<unsigned N, unsigned P, enum page_state S>
void allocator_cb<N, P, S>::free(uint64_t index) {
    auto& b = bitmap[index / 64];
    unsigned int bit = 63 - (index & 63);

    assert(b & (1 << bit));

    b &= ~(1 << bit);

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

    free_folio(pfns[index / BITMAP_BITS_PER_PAGE] << PAGE_SHIFT);
#ifdef DEBUG
    pfns[index / BITMAP_BITS_PER_PAGE] = 0;
#endif
}

template<unsigned N, unsigned P, enum page_state S>
void* ::allocator<N, P, S>::malloc() {
    // FIXME - move calls to allocate_page out of this object's spinlock

    lock_guard l(lock);

    for (auto le = control_blocks.next; le != &control_blocks; le = le->next) {
        auto& cb = containing_record<cb_type>(le);

        auto pa = cb.malloc_pa();
        if (pa)
            return phys_address(pa);
    }

    auto cb = (cb_type*)phys_address(allocate_page());

    new (cb) cb_type;

    insert_tail_list(control_blocks, cb->le);

    auto pa = cb->malloc_pa();
    if (!pa)
        return nullptr;

    return phys_address(pa);
}

template<unsigned N, unsigned P, enum page_state S>
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

    lock_guard l(lock);

    cb.free(index);
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
    __asm__ __volatile__ (
        ""
        :
        :
        : "memory"
    );
}

export constexpr uint8_t bytes_to_folio_order(size_t s) {
    uint32_t p = bytes_to_pages(s);

    if (p == 0)
        return 0;

    return (uint8_t)(32 - countl_zero(p - 1));
}
