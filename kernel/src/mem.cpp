#include "../../efi/src/loaderblock.h"
#include "kernel.h"
#include "acpi.h"
#include "mem.h"
#include "spinlock.h"
#include <span>
#include <cstring>

using namespace std;

static uint8_t* memblock = (uint8_t*)0xfffffa0000000000;
static uint64_t memblock_size;

void* acpi_tables = (void*)0xfffffad000000000;
unsigned int acpi_count = 0;

static const uint64_t heap_block = 0xfffffaf000000000;
static unsigned int heap_pages = 0;

static uint64_t* const vablock = (uint64_t*)0xfffffa4000000000;
static uint64_t vablock_size;

static spinlock pa_spinlock, va_spinlock, heap_spinlock;

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

uint64_t allocate_page() {
    // FIXME - SSE?
    // FIXME - BMI1 instruction set?

    auto mb = span((uint64_t*)memblock, memblock_size / sizeof(uint64_t));

    lock_guard lock(pa_spinlock);

    for (unsigned int i = 0; i < mb.size(); i++) {
        if (mb[i] != 0) {
            uint64_t v = mb[i];

            for (uint64_t j = 0; j < 64; j++) {
                if (v & 1) {
                    mb[i] &= ~((uint64_t)1 << j);
                    j = (j & ~7) + 7 - (j & 7);
                    return ((i * 64) + j) << PAGE_SHIFT;
                }

                v >>= 1;
            }
        }
    }

    return 0;
}

bool map_page(uint64_t pa, uint64_t va, enum mapping_type type) {
    auto pml4 = span((HARDWARE_PTE*)SELFMAP_PML4, 512);
    auto pdpt = span((HARDWARE_PTE*)(SELFMAP_PDPT | (((va & 0xff8000000000) >> 39) << PAGE_SHIFT)), 512);
    auto pdt = span((HARDWARE_PTE*)(SELFMAP_PDT | (((va & 0xffffc0000000) >> 30) << PAGE_SHIFT)), 512);
    auto pt = span((HARDWARE_PTE*)(SELFMAP | (((va & 0xffffffe00000) >> 21) << PAGE_SHIFT)), 512);

    auto& pml4_entry = pml4[(va & 0xff8000000000) >> 39];

    if (!pml4_entry.Valid) {
        uint64_t page = allocate_page();
        if (page == 0)
            return false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pml4_entry.PageFrameNumber = page >> PAGE_SHIFT;
#pragma GCC diagnostic pop
        pml4_entry.Valid = 1;
        pml4_entry.Write = 1;
        pml4_entry.Owner = va & 0x8000000000000000 ? 0 : 1;

        memset(pdpt.data(), 0, PAGE_SIZE);
    }

    auto& pdpt_entry = pdpt[(va & 0x7fc0000000) >> 30];

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

        memset(pdt.data(), 0, PAGE_SIZE);
    }

    auto& pdt_entry = pdt[(va & 0x3fe00000) >> 21];

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

        memset(pt.data(), 0, PAGE_SIZE);
    }

    auto& pt_entry = pt[(va & 0x1ff000) >> PAGE_SHIFT];

    *(uint64_t*)&pt_entry = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pt_entry.PageFrameNumber = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pt_entry.Valid = 1;

    switch (type) {
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
    }

    pt_entry.Owner = va & 0x8000000000000000 ? 0 : 1;

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

    lock_guard lock(pa_spinlock);

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

    lock_guard lock(pa_spinlock);

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

static bool copy_acpi(const loader_block& lb) {
    auto& xsdt = *(ACPISDTHeader*)lb.xsdt_pa;
    uint32_t needed = 0;

    // checksum of XSDT already checked by bootloader

    auto entries = span((uint64_t*)((uint8_t*)&xsdt + sizeof(ACPISDTHeader)), (xsdt.Length - sizeof(ACPISDTHeader)) / sizeof(uint64_t));

    for (auto e : entries) {
        auto& tbl = *(ACPISDTHeader*)(uintptr_t)e;
        uint8_t csum = 0;

        for (unsigned int i = 0; i < tbl.Length; i++) {
            csum += ((uint8_t*)&tbl)[i];
        }

        if (csum == 0)
            needed += tbl.Length;
    }

    uint32_t needed_pages = (needed + PAGE_SIZE - 1) >> PAGE_SHIFT;

    for (unsigned int i = 0; i < needed_pages; i++) {
        auto pa = allocate_page();
        if (pa == 0)
            return false;

        if (!map_page(pa, ((uintptr_t)acpi_tables) + (i << PAGE_SHIFT), mapping_type::read_write))
            return false;
    }

    auto ptr = (uint8_t*)acpi_tables;

    for (auto e : entries) {
        auto& tbl = *(ACPISDTHeader*)(uintptr_t)e;
        uint8_t csum = 0;

        for (unsigned int i = 0; i < tbl.Length; i++) {
            csum += ((uint8_t*)&tbl)[i];
        }

        if (csum == 0) {
            memcpy(ptr, &tbl, tbl.Length);
            ptr += tbl.Length;
            acpi_count++;
        }
    }

    return true;
}

void free_lower_half_mappings() {
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

    memblock_size = num_memblock_pages << PAGE_SHIFT;
    memset(memblock, 0, memblock_size);

    // populate memblock from loader block

    // 0 = reserved, 1 = free
    for (auto& map : map_entries) {
        if (map.type == map_type::mem_free)
            mark_mem_as_free(map.addr, map.len);
    }

    // subtract memblock itself, and scratch that we've used
    mark_mem_as_used(memblock_pa, (num_memblock_pages + (orig_scratch_pages - scratch_pages)) << PAGE_SHIFT);

    copy_acpi(lb);

    // mark bootloader memory as free (inc. page tables)
    for (auto& map : map_entries) {
        if (map.type == map_type::mem_bootloader)
            mark_mem_as_free(map.addr, map.len);
    }

    return true;
}

extern "C"
void* malloc(size_t size) {
    // round to multiple of 4
    if (size & 3)
        size = ((size >> 2) + 1) << 2;

    if (size == 0 || size >= 0x80000000)
        return nullptr;

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

                if (h->size == size) {
                    h->free = 0;
                    return (uint8_t*)h + sizeof(alloc_header);
                }

                if (h->size > size) {
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
                    return (uint8_t*)h + sizeof(alloc_header);
                }
            }

            last = h;
            h = (alloc_header*)((uint8_t*)h + sizeof(alloc_header) + h->size);
        } while (h < (void*)(heap_block | (heap_pages << PAGE_SHIFT)));

        if (last && last->free)
            ah = last;
    }

    // allocate new pages

    unsigned int new_pages = bytes_to_pages(sizeof(alloc_header) + size);

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
void free(void* ptr) {
    auto ah = (alloc_header*)((uint8_t*)ptr - sizeof(alloc_header));

    ah->free = 1;
}

void* operator new(size_t count) noexcept {
    return malloc(count);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

void free_page(uint64_t pa) {
    pa >>= PAGE_SHIFT;

    lock_guard lock(pa_spinlock);

    auto mb = span((uint64_t*)memblock, memblock_size / sizeof(uint64_t));

    uint64_t i = pa / 64;
    uint64_t j = pa % 64;

    mb[i] |= (uint64_t)1 << (((j >> 3) << 3) + 7 - (j & 7));
}

void unmap_and_free_page(uint64_t va) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    if (!pt_entry.Valid)
        return;

    auto pa = ((uint64_t)pt_entry.PageFrameNumber) << PAGE_SHIFT;

    free_page(pa);
}

void unmap_page(uint64_t va) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    pt_entry.Valid = 0;

    __asm __volatile(
        "invlpg %0"
        :
        : "m" (va)
    );
}

void protect_page(uint64_t va, enum mapping_type type) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    switch (type) {
        case mapping_type::read_write:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
            pt_entry.CacheDisable = 0;
        break;

        case mapping_type::read_only:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 0;
            pt_entry.CacheDisable = 0;
        break;

        case mapping_type::read_execute:
            pt_entry.NoExecute = 0;
            pt_entry.Write = 0;
            pt_entry.CacheDisable = 0;
        break;

        case mapping_type::read_write_execute:
            pt_entry.NoExecute = 0;
            pt_entry.Write = 1;
            pt_entry.CacheDisable = 0;
        break;

        case mapping_type::no_cache:
            pt_entry.NoExecute = 1;
            pt_entry.Write = 1;
            pt_entry.CacheDisable = 1;
        break;
    }
}

static void mark_vamem_as_used(uint64_t off, size_t pages) {
    if (off & 63) {
        auto& b = vablock[off >> 6];

        while (off & 63 && pages != 0) {
            b &= ~((uint64_t)1 << (63 - (off & 63)));
            off++;
            pages--;
        }
    }

    if (pages == 0)
        return;

    memset(vablock + (off >> 6), 0, (pages >> 6) * sizeof(uint64_t));

    if (pages & 63) {
        off += pages & ~63;
        pages &= 63;

        auto& b = vablock[off >> 6];

        while (pages != 0) {
            b &= ~((uint64_t)1 << (63 - (off & 63)));
            off++;
            pages--;
        }
    }
}

bool init_vablock() {
    vablock_size = memblock_size * 2;

    // VA block reserved is 64 GB, don't go over this
    if (vablock_size > 0x200000)
        vablock_size = 0x200000;

    auto pages = vablock_size >> PAGE_SHIFT;
    auto va = (uintptr_t)vablock;

    for (unsigned int i = 0; i < pages; i++) {
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

    mark_vamem_as_used(0, pages);

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

uint64_t allocate_va(uint32_t pages) {
    auto mb = span((uint64_t*)vablock, vablock_size / sizeof(uint64_t));
    uint64_t run_start = 0;
    uint64_t run_length = 0;
    uint64_t mask;

    if (pages < 64)
        mask = (((uint64_t)1 << pages) - 1) << (uint64_t)(64 - pages);

    lock_guard lock(va_spinlock);

    // start from where we left off last time, to reduce TLB caching issues

    static uint64_t last_i = 0, last_j = 0;

    for (uint64_t i = last_i; i < mb.size(); i++) {
        if (mb[i] & 0x8000000000000000 && last_j == 0) {
            if (run_length == 0)
                run_start = i * 64;

            run_length += leading_ones(mb[i]);

            if (run_length >= pages) {
                mark_vamem_as_used(run_start, pages);
                last_i = i;
                last_j = ((run_start + pages) % 64) + 1;
                return (uintptr_t)vablock + (run_start << PAGE_SHIFT);
            }
        } else
            run_length = 0;

        if (pages < 64) {
            auto m = mask;
            for (uint64_t j = 0; j < pages; j++) {
                if (j >= last_j && (mb[i] & m) == m) {
                    mb[i] &= ~m;
                    last_i = i;
                    last_j = j + 1;
                    return (uintptr_t)vablock + (((i * 64) + j) << PAGE_SHIFT);
                }

                m >>= 1;
            }
        }

        last_j = 0;

        if (mb[i] == 0xffffffffffffffff)
            continue;

        auto t1 = trailing_ones(mb[i]);
        run_length = t1;
        run_start = ((i + 1) * 64) - t1;
    }

    // FIXME - if we reach the end, issue IPIs to clear TLBs on other CPUs, and start again from beginning

    if (run_length >= pages) {
        mark_vamem_as_used(run_start, pages);
        return (uintptr_t)vablock + (run_start << PAGE_SHIFT);
    }

    return 0;
}

void free_va(uint64_t va, uint32_t pages) {
    va -= (uintptr_t)vablock;
    va >>= PAGE_SHIFT;

    lock_guard lock(va_spinlock);

    auto mb = span((uint64_t*)vablock, vablock_size / sizeof(uint64_t));

    for (unsigned int n = 0; n < pages; n++) {
        uint64_t i = va / 64;
        uint64_t j = 63 - (va % 64);

        mb[i] |= (uint64_t)1 << j;

        va++;
    }
}

uint64_t va_to_pa(uint64_t va) {
    auto& pt_entry = *(HARDWARE_PTE*)(SELFMAP | (((va & 0xfffffffff000) >> PAGE_SHIFT) << 3));

    return (uint64_t)pt_entry.PageFrameNumber << PAGE_SHIFT;
}

void invalidate_tlb() {
    __asm __volatile (
        "mov rax, cr3\n"
        "mov cr3, rax\n"
        :
        :
        : "rax"
    );
}

void* memchr(const void* ptr, int ch, size_t count) {
    auto p = (uint8_t*)ptr;

    for (size_t i = 0; i < count; i++) {
        if (*p == ch)
            return p;

        p++;
    }

    return nullptr;
}

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

        i++;
    }

    return 0;
}
