module;

#include <loaderblock.h>
#include "kernel.h"
#include "acpica.h"
#include "mem.h"
#include "spinlock.h"
#include <span>
#include <cstring>

export module mem;

import print;
import bsod;
import config;
import list;

using namespace std;

export enum class page_state {
    free,
    used,
    allocator_4k,
    allocator_2k,
    allocator_1k,
    allocator_512,
    allocator_256,
    allocator_128,
    allocator_64
};

struct page {
    union {
        struct {
            list_entry le;
        } free;
        struct {
            void* cb;
            unsigned int index;
        } allocator;
    };

    page_state state;
};

page* pages = (page*)0xfffffa0000000000;
static uint64_t total_pages, free_pages;
static spinlock pa_spinlock;
static list_entry free_page_list = { &free_page_list, &free_page_list };

static const uint64_t heap_block = 0xfffffaf000000000;
static unsigned int heap_pages = 0;

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

class allocator_4k {
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

static allocator_4k alloc4k;
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

export uint64_t allocate_page(enum page_state state = page_state::used) {
    lock_guard lock(pa_spinlock);

    if (is_list_empty(free_page_list))
        return 0;

    auto& p = *(page*)((uint8_t*)free_page_list.next - offsetof(page, free.le));

    remove_entry_list(p.free.le);
    p.state = state;

    free_pages--;

    return (uint64_t)(&p - pages) << PAGE_SHIFT;
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
            insert_tail_list(free_page_list, pages[pfn].free.le);
            pages[pfn].state = page_state::free;
            free_pages++;
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

export bool init_memory(const loader_block& lb) {
    uint64_t num_memblock_bytes, num_memblock_pages, orig_scratch_pages, scratch_pages, needed_pages;
    unsigned int num_map_entries = lb.num_map_entries;
    uint64_t memblock_pa;
    auto lb_map = (map_entry*)lb.map_ptr;

    init_pat();

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

    scratch_pages = orig_scratch_pages = 5; // pages needed for paging tables
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

static void* malloc_aligned(size_t size, size_t align) {
    // round to multiple of 4
    if (size & 3)
        size = ((size >> 2) + 1) << 2;

    if (size == 0 || size >= 0x80000000)
        return nullptr;

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

    alloc_header* ah = nullptr;

    lock_guard lock(heap_spinlock);

    // allocate from existing pages if possible

    if (heap_pages > 0) {
        auto h = (alloc_header*)heap_block;
        alloc_header* last = nullptr;

        do {
            if (h->free) {
                if (last && last->free) { // merge adjacent free entries
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    last->size += h->size + sizeof(alloc_header);
#pragma GCC diagnostic pop
                    h = last;
                }

                auto ptr = (uint8_t*)h + sizeof(alloc_header);

                if (h->size == size && !((uintptr_t)ptr & (align - 1))) {
                    h->free = 0;
                    return ptr;
                }

                if (h->size > size && !((uintptr_t)ptr & (align - 1))) {
                    auto nh = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + size);

                    if (nh < (void*)(heap_block | (heap_pages << PAGE_SHIFT))) {
                        nh->free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        nh->size = h->size - size - sizeof(alloc_header);
#pragma GCC diagnostic pop
                    }

                    h->free = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    h->size = size;
#pragma GCC diagnostic pop
                    return ptr;
                }

                if (align != 1) {
                    auto padding = ((uintptr_t)ptr & ~(align - 1)) + align - (uintptr_t)ptr;

                    if (h->size >= size + padding) {
                        auto h_size = h->size;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        h->size = padding - sizeof(alloc_header);
#pragma GCC diagnostic pop

                        h = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + h->size);
                        ptr = (uint8_t*)h + sizeof(alloc_header);

                        h->free = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        h->size = size;
#pragma GCC diagnostic pop

                        auto nh = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + size);

                        if (nh < (void*)(heap_block | (heap_pages << PAGE_SHIFT))) {
                            nh->free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                            nh->size = h_size - size - sizeof(alloc_header) - padding;
#pragma GCC diagnostic pop
                        }

                        return ptr;
                    }
                }
            }

            last = h;
            h = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + h->size);
        } while (h < (void*)(heap_block | (heap_pages << PAGE_SHIFT)));

        if (last && last->free)
            ah = last;
    }

    // allocate new pages

    unsigned int new_pages = bytes_to_pages(sizeof(alloc_header) + size + align - 1);

    for (unsigned int i = 0; i < new_pages; i++) {
        // FIXME - die if heap_pages too large for virtual address

        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");
            return nullptr;
        }

        auto va = (void*)(heap_block | (heap_pages << PAGE_SHIFT));

        if (!map_page(pa, (uintptr_t)va, mapping_type::read_write)) {
            print("map_page failed\n");
            return nullptr;
        }

        invalidate_tlb(va);

        if (ah) {
            ah->size = (uint32_t)ah->size + PAGE_SIZE;
            // FIXME - handle case when size would overflow 31 bits
        } else {
            ah = (alloc_header*)va;
            ah->free = 1;
            ah->size = PAGE_SIZE - sizeof(alloc_header);
        }

        heap_pages++;
    }

    auto ptr = (uint8_t*)ah + sizeof(alloc_header);

    if ((uintptr_t)ptr & (align - 1)) {
        auto padding = ((uintptr_t)ptr & ~(align - 1)) + align - (uintptr_t)ptr;

        ah->free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        ah->size = padding - sizeof(alloc_header);
#pragma GCC diagnostic pop

        ah = (alloc_header*)((uint8_t*)ah + sizeof(alloc_header) + ah->size);
        ptr = (uint8_t*)ah + sizeof(alloc_header);
    }

    ah->free = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    ah->size = size;
#pragma GCC diagnostic pop

    auto ah2 = (alloc_header*)((uint8_t*)ptr + size);

    if ((uintptr_t)ah2 & (PAGE_SIZE - 1)) {
        ah2->free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        ah2->size = (uint32_t)((heap_block | (heap_pages << PAGE_SHIFT)) - (uintptr_t)ah2 - sizeof(alloc_header));
#pragma GCC diagnostic pop
    }

    return ptr;
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

constexpr uint64_t phys_memory_pfn(void* ptr) {
    return ((uintptr_t)ptr & ~PHYS_MEMORY_MASK) >> PAGE_SHIFT;
}

extern "C"
void free(void* ptr) {
    if (!ptr)
        return;

    if (is_phys_memory(ptr)) {
        auto& page = pages[phys_memory_pfn(ptr)];

        switch (page.state) {
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
    }

    auto ah = (alloc_header*)((uint8_t*)ptr - sizeof(alloc_header));

    ah->free = 1;
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

export void free_page(uint64_t pa) {
    auto pfn = pa >>= PAGE_SHIFT;

    lock_guard lock(pa_spinlock);

    if (pages[pfn].state == page_state::free) // FIXME - warning for this
        return;

    insert_tail_list(free_page_list, pages[pfn].free.le);
    pages[pfn].state = page_state::free;

    free_pages++;
}

export void unmap_and_free_page(uint64_t va) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    if (!pt_entry.Valid)
        return;

    auto pa = ((uint64_t)pt_entry.PageFrameNumber) << PAGE_SHIFT;

    free_page(pa);

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

export uint64_t va_to_pa(uint64_t va) {
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

void* allocator_4k::malloc() {
    auto p = allocate_page(page_state::allocator_4k);

    if (!p)
        return nullptr;

    return phys_address(p);
}

void allocator_4k::free(void* ptr) {
    auto pfn = phys_memory_pfn(ptr);

    free_page(pfn << PAGE_SHIFT);
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

    free_page(pfn << PAGE_SHIFT);

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

    free_page(pfns[index / BITMAP_BITS_PER_PAGE]);
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
