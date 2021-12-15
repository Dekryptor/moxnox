#include "../../bootloader/src/loaderblock.h"
#include "kernel.h"
#include "acpi.h"
#include <span>
#include <cstring>

using namespace std;

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
uint64_t memblock_size;
void* gdt = (void*)0xfffffa8000000000;

void* acpi_tables = (void*)0xfffffad000000000;
unsigned int acpi_count = 0;

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

void* memcpy(void* dest, const void* src, size_t n) {
    // FIXME - SSE version of this?

    while (n >= sizeof(uint64_t)) {
        *(uint64_t*)dest = *(uint64_t*)src;

        dest = (uint8_t*)dest + sizeof(uint64_t);
        src = (uint8_t*)src + sizeof(uint64_t);

        n -= sizeof(uint64_t);
    }

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

    return dest;
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

uint64_t allocate_page() {
    // FIXME - SSE?
    // FIXME - BMI1 instruction set?

    // FIXME - need spinlock

    auto mb = span((uint64_t*)memblock, memblock_size / sizeof(uint64_t));

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

bool map_page(uint64_t pa, uint64_t va) {
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

        memset(pt.data(), 0, PAGE_SIZE);
    }

    auto& pt_entry = pt[(va & 0x1ff000) >> PAGE_SHIFT];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pt_entry.PageFrameNumber = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    pt_entry.Valid = 1;
    pt_entry.Write = 1;
    // FIXME - option to set NX bit etc.

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

static bool copy_gdt() {
    uint64_t pa;

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdt_ptr;

    pa = allocate_page();
    if (pa == 0)
        return false;

    if (!map_page(pa, (uintptr_t)gdt))
        return false;

    __asm__ __volatile__ (
        "sgdt %0"
        : "=m" (gdt_ptr)
    );

    memcpy(gdt, (void*)gdt_ptr.base, gdt_ptr.limit + 1);

    gdt_ptr.base = (uintptr_t)gdt;

    __asm__ __volatile__ (
        "lgdt %0\n"
        "push 0x8\n"
        "lea rax, [rip+label]\n"
        "push rax\n"
        "retfq\n"
        "label:\n"
        :
        : "m" (gdt_ptr)
        : "rax"
    );

    return true;
}

static bool copy_acpi(const loader_block& lb) {
    auto& rsdt = *(ACPISDTHeader*)lb.rsdt_pa;
    uint32_t needed = 0;

    // checksum of RSDT already checked by bootloader

    auto entries = span((uint32_t*)((uint8_t*)&rsdt + sizeof(ACPISDTHeader)), (rsdt.Length - sizeof(ACPISDTHeader)) / sizeof(uint32_t));

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

    auto pa = allocate_page();
    if (pa == 0)
        return false;

    for (unsigned int i = 0; i < needed_pages; i++) {
        if (!map_page(pa + (i << PAGE_SHIFT), ((uintptr_t)acpi_tables) + (i << PAGE_SHIFT)))
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

static void free_lower_half_mappings() {
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

    copy_gdt();

    copy_acpi(lb);

    // mark bootloader memory as free (inc. page tables)
    for (auto& map : map_entries) {
        if (map.type == map_type::mem_bootloader)
            mark_mem_as_free(map.addr, map.len);
    }

    free_lower_half_mappings();

    // FIXME - protect pages of kernel sections correctly

    return true;
}
