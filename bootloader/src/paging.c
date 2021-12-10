#include "bootloader.h"

#define PAGE_SIZE 4096

HARDWARE_PTE* pml4t;

static map_entry map_copy[256];
static unsigned int num_map_entries_copy;

static void* allocate_page() {
    // FIXME - make sure we only grab a physical address within first 4GB

    for (unsigned int i = 0; i < num_map_entries; i++) {
        if (map[i].type == mem_free && map[i].len >= PAGE_SIZE) {
            void* addr = (void*)(uintptr_t)map[i].addr;

            if (!add_map_entry(map[i].addr, PAGE_SIZE, mem_paging_table))
                return NULL;

            return addr;
        }
    }

    return NULL;
}

static bool map_page(uint64_t va, uint64_t pa) {
    uint32_t pml4t_offset, pdpt_offset, pdt_offset, pt_offset;
    HARDWARE_PTE *pdpt, *pdt, *pt;

    pml4t_offset = (va >> 39) & 0x1ff;

    if (!pml4t[pml4t_offset].Valid) {
        void* new_page = allocate_page();

        if (!new_page) {
            print("Could not allocate page for memory mapping.\n");
            return false;
        }

        memset(new_page, 0, PAGE_SIZE);

        pml4t[pml4t_offset].Valid = 1;
        pml4t[pml4t_offset].Write = 1;
        pml4t[pml4t_offset].PageFrameNumber = ((uintptr_t)new_page) >> 12;
    }

    pdpt = (HARDWARE_PTE*)((uintptr_t)pml4t[pml4t_offset].PageFrameNumber << 12);

    pdpt_offset = (va >> 30) & 0x1ff;

    if (!pdpt[pdpt_offset].Valid) {
        void* new_page = allocate_page();

        if (!new_page) {
            print("Could not allocate page for memory mapping.\n");
            return false;
        }

        memset(new_page, 0, PAGE_SIZE);

        pdpt[pdpt_offset].Valid = 1;
        pdpt[pdpt_offset].Write = 1;
        pdpt[pdpt_offset].PageFrameNumber = ((uintptr_t)new_page) >> 12;
    }

    pdt = (HARDWARE_PTE*)((uintptr_t)pdpt[pdpt_offset].PageFrameNumber << 12);

    pdt_offset = (va >> 21) & 0x1ff;

    if (!pdt[pdt_offset].Valid) {
        void* new_page = allocate_page();

        if (!new_page) {
            print("Could not allocate page for memory mapping.\n");
            return false;
        }

        memset(new_page, 0, PAGE_SIZE);

        pdt[pdt_offset].Valid = 1;
        pdt[pdt_offset].Write = 1;
        pdt[pdt_offset].PageFrameNumber = ((uintptr_t)new_page) >> 12;
    }

    pt = (HARDWARE_PTE*)((uintptr_t)pdt[pdt_offset].PageFrameNumber << 12);

    pt_offset = (va >> 12) & 0x1ff;

    if (pt[pt_offset].Valid) {
        print("Mapping for ");
        print_hex(va);
        print(" already set.\n");
        return false;
    }

    pt[pt_offset].Valid = 1;
    pt[pt_offset].Write = 1;
    pt[pt_offset].PageFrameNumber = pa >> 12;

    return true;
}

static bool do_map(uint64_t va, uint64_t pa, uint32_t len) {
    while (len > 0) {
        if (!map_page(va, pa))
            return false;

        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        len -= PAGE_SIZE;
    }

    return true;
}

bool setup_paging(uint64_t va) {
    pml4t = allocate_page();
    if (!pml4t) {
        print("Could not allocate page for PML4T.\n");
        return false;
    }

    memset(pml4t, 0, PAGE_SIZE);

    memcpy(map_copy, map, sizeof(map_entry) * num_map_entries);
    num_map_entries_copy = num_map_entries;

    for (unsigned int i = 0; i < num_map_entries_copy; i++) {
        if (map_copy[i].type == mem_bootloader) {
            if (!do_map(map_copy[i].addr, map_copy[i].addr, map_copy[i].len)) // identity map
                return false;
        } else if (map_copy[i].type == mem_image) {
            if (!do_map(va, map_copy[i].addr, map_copy[i].len)) // map to high part
                return false;
        }
    }

    return true;
}
