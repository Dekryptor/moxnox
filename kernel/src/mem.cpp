#include "../../bootloader/src/loaderblock.h"
#include "kernel.h"
#include <span>
#include <cstring>

using namespace std;

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define SELFMAP                 0xfffff68000000000
#define SELFMAP_PDT             0xfffff6fb40000000
#define SELFMAP_PDPT            0xfffff6fb7da00000
#define SELFMAP_PML4            0xfffff6fb7dbed000

typedef struct {
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
} HARDWARE_PTE;

uint8_t* memblock = (uint8_t*)0xfffffa0000000000;

static bool treat_as_free(enum map_type type) {
    switch (type) {
        case map_type::mem_free:
        case map_type::mem_bootloader:
        case map_type::mem_paging_table:
            return true;

        default:
            return false;
    }
}

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
//     pt_entry.NoExecute = 1; // FIXME - why doesn't NX let us write?

    return true;
}

static bool map_memblock(uint64_t pa, uint64_t pages, uint64_t& scratch_pages) {
    uint64_t scratch_pa = pa + (pages << PAGE_SHIFT);
    uint64_t va = (uintptr_t)memblock;

    while (pages > 0) {
        if (!map_page_with_scratch(va, pa, scratch_pa, scratch_pages))
            return false;

        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        pages--;
    }

    return true;
}

static void mark_mem_as_free(uint64_t pa, size_t len) {
    uint64_t off = pa >> PAGE_SHIFT;
    uint64_t pages = len >> PAGE_SHIFT;

    if (off & 7) {
        auto& b = memblock[off >> 3];

        while (off & 7 && pages != 0) {
            b |= (uint8_t)(1 << (7 - (off & 7)));
            off++;
            pages--;
        }
    }

    if (pages == 0)
        return;

    memset(memblock + (off >> 3), 0xff, pages >> 3);

    if (pages & 7) {
        off += pages & ~7;
        pages &= 7;

        auto& b = memblock[off >> 3];

        while (pages != 0) {
            b |= (uint8_t)(1 << (7 - (off & 7)));
            off++;
            pages--;
        }
    }
}

static void mark_mem_as_used(uint64_t pa, size_t len) {
    uint64_t off = pa >> PAGE_SHIFT;
    uint64_t pages = len >> PAGE_SHIFT;

    if (off & 7) {
        auto& b = memblock[off >> 3];

        while (off & 7 && pages != 0) {
            b &= (uint8_t)~(1 << (7 - (off & 7)));
            off++;
            pages--;
        }
    }

    if (pages == 0)
        return;

    memset(memblock + (off >> 3), 0, pages >> 3);

    if (pages & 7) {
        off += pages & ~7;
        pages &= 7;

        auto& b = memblock[off >> 3];

        while (pages != 0) {
            b &= (uint8_t)~(1 << (7 - (off & 7)));
            off++;
            pages--;
        }
    }
}

bool init_memory(const loader_block& lb) {
    uint64_t total_pages, num_memblock_bytes, num_memblock_pages, orig_scratch_pages, scratch_pages, needed_pages;
    unsigned int num_map_entries = lb.num_map_entries;
    uint64_t memblock_pa;

    // ignore any non-free entries at end of table
    while (num_map_entries > 0 && !treat_as_free(lb.map[num_map_entries - 1].type)) {
        num_map_entries--;
    }

    if (num_map_entries == 0)
        return false;

    auto map_entries = span(lb.map, num_map_entries);

    auto& m = map_entries[num_map_entries - 1];
    total_pages = (m.addr + m.len) >> PAGE_SHIFT;

    num_memblock_bytes = (total_pages + 7) / 8;
    num_memblock_pages = (num_memblock_bytes + PAGE_SIZE - 1) >> PAGE_SHIFT;

    scratch_pages = orig_scratch_pages = 5; // pages needed for paging tables
    needed_pages = num_memblock_pages + scratch_pages;

    // allocate physical pages for memblock

    memblock_pa = 0;
    for (auto& map : map_entries) {
        if (map.type == map_type::mem_free && map.len >= (needed_pages << PAGE_SHIFT)) {
            memblock_pa = map.addr;
            break;
        }
    }

    if (memblock_pa == 0)
        return false;

    if (!map_memblock(memblock_pa, num_memblock_pages, scratch_pages))
        return false;

    memset(memblock, 0, num_memblock_pages << PAGE_SHIFT);

    // populate memblock from loader block

    // 0 = reserved, 1 = free
    for (auto& map : map_entries) {
        if (map.type == map_type::mem_free)
            mark_mem_as_free(map.addr, map.len);
    }

    // subtract memblock itself, and scratch that we've used
    mark_mem_as_used(memblock_pa, (num_memblock_pages + (orig_scratch_pages - scratch_pages)) << PAGE_SHIFT);

    // FIXME - copy GDT to new page

    // FIXME - mark bootloader memory as free (inc. page tables)
    // FIXME - free lower half mappings

    // FIXME - protect pages of kernel sections correctly

    return true;
}
