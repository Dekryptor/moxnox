#include <stdint.h>
#include "efi.h"

map_entry map[256];
unsigned int num_map_entries = 0;

static const uint64_t SELFMAP = 0xfffff68000000000;
static const uint64_t KERNEL_STACK = 0xfffff90000000000;

static HARDWARE_PTE* pml4t;

extern "C"
void* memcpy(void* dest, const void* src, size_t n) {
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

extern "C"
void* memset(void* s, int c, size_t n) {
    void* orig_s = s;

    // FIXME - faster if we make sure we're aligned (also in memcpy)?

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

void print_hex(uint64_t num) {
    bool first = true;
    char16_t s[2];

    if (num == 0) {
        print(u"0");
        return;
    }

    s[1] = 0;

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                s[0] = (char16_t)(n + u'0');
            else
                s[0] = (char16_t)(n - 0xa + u'a');

            print(s);
            first = false;
        }

        num <<= 4;
    }
}

static enum map_type map_efi_type(EFI_MEMORY_TYPE Type) {
    switch (Type) {
        case EfiReservedMemoryType:
        case EfiUnusableMemory:
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
        case EfiPalCode:
            return map_type::mem_reserved;

        case EfiLoaderCode:
        case EfiLoaderData:
            return map_type::mem_bootloader;

        case EfiRuntimeServicesCode:
            return map_type::mem_uefi_code;

        case EfiRuntimeServicesData:
            return map_type::mem_uefi_data;

        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
            return map_type::mem_free;

        case EfiACPIMemoryNVS:
        case EfiACPIReclaimMemory:
            return map_type::mem_acpi;

        default:
            return map_type::mem_reserved;
    }
}

static void insert_map_entry(int i) {
    // FIXME - memmove?
    for (int j = num_map_entries - 1; j >= i; j--) {
        memcpy(&map[j+1], &map[j], sizeof(map_entry));
    }
}

EFI_STATUS get_memory_map() {
    EFI_STATUS status;
    UINTN map_size, key, descriptor_size;
    UINT32 descriptor_version;
    EFI_MEMORY_DESCRIPTOR* memory_map = nullptr;

    map_size = 0;

    status = bs->GetMemoryMap(&map_size, memory_map, &key, &descriptor_size, &descriptor_version);
    if (status == EFI_SUCCESS) {
        print(u"GetMemoryMap did not return memory map.\r\n");
        return EFI_INVALID_PARAMETER;
    } else if (status != EFI_BUFFER_TOO_SMALL) {
        print_error(u"GetMemoryMap", status);
        return status;
    }

    map_size *= 2; // some leeway, because AllocatePool can change things

    status = bs->AllocatePool(EfiBootServicesData, map_size, (void**)&memory_map);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return status;
    }

    status = bs->GetMemoryMap(&map_size, memory_map, &key, &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        print_error(u"GetMemoryMap", status);
        bs->FreePool(memory_map);
        return status;
    }

    auto ptr = (uint8_t*)memory_map;
    auto left = map_size;

    while (left > descriptor_size) {
        auto& m = *(EFI_MEMORY_DESCRIPTOR*)ptr;
        auto& me = map[num_map_entries];

        if (num_map_entries == sizeof(map) / sizeof(map[0])) {
            print(u"Too many entries in memory map.\r\n");
            bs->FreePool(memory_map);
            return EFI_INVALID_PARAMETER;
        }

        me.addr = m.PhysicalStart;
        me.len = m.NumberOfPages << EFI_PAGE_SHIFT;
        me.type = map_efi_type((EFI_MEMORY_TYPE)m.Type);
        num_map_entries++;

        if (left == descriptor_size)
            break;

        ptr += descriptor_size;
        left -= descriptor_size;
    }

    bs->FreePool(memory_map);

    // FIXME - do we need to order memory map, and resolve any overlaps?

    // add missing entries
    if (map[0].addr != 0) {
        if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
            print(u"Too many entries in memory map.\r\n");
            return EFI_INVALID_PARAMETER;
        }

        insert_map_entry(0);

        map[0].addr = 0;
        map[0].len = map[1].addr;
        map[0].type = map_type::mem_reserved;
    } else {
        if (map[0].type != map_type::mem_reserved) { // snatch first page
            insert_map_entry(0);

            map[0].addr = 0;
            map[0].len = EFI_PAGE_SIZE;
            map[0].type = map_type::mem_reserved;

            map[1].addr += EFI_PAGE_SIZE;
            map[1].len -= EFI_PAGE_SIZE;
        }
    }

    for (unsigned int i = 0; i < num_map_entries - 1; i++) {
        if (map[i].addr + map[i].len < map[i+1].addr) {
            if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
                print(u"Too many entries in memory map.\r\n");
                return EFI_INVALID_PARAMETER;
            }

            insert_map_entry(i+1);

            map[i+1].addr = map[i].addr + map[i].len;
            map[i+1].len = map[i+2].addr - map[i+1].addr;
            map[i+1].type = map_type::mem_reserved;

            num_map_entries++;
        }
    }

    // merge entries with same type
    for (unsigned int i = 1; i < num_map_entries; i++) {
        auto& me1 = map[i - 1];
        auto& me2 = map[i];

        if (me2.addr == me1.addr + me1.len && me2.type == me1.type) {
            me1.len += me2.len;
            memcpy(&map[i], &map[i+1], (num_map_entries - i - 1) * sizeof(map_entry));
            i--;
            num_map_entries--;
        }
    }

    // remove final entry if inaccessible
    if (map[num_map_entries - 1].type == map_type::mem_reserved)
        num_map_entries--;

    return EFI_SUCCESS;
}

static EFI_STATUS map_page(uint64_t va, uint64_t pa) {
    EFI_STATUS status;
    uint32_t pml4t_offset, pdpt_offset, pdt_offset, pt_offset;
    HARDWARE_PTE *pdpt, *pdt, *pt;

    pml4t_offset = (va >> 39) & 0x1ff;

    if (!pml4t[pml4t_offset].Valid) {
        EFI_PHYSICAL_ADDRESS new_page;

        status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &new_page);
        if (EFI_ERROR(status)) {
            print_error(u"AllocatePages", status);
            return status;
        }

        memset((void*)new_page, 0, EFI_PAGE_SIZE);

        pml4t[pml4t_offset].Valid = 1;
        pml4t[pml4t_offset].Write = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pml4t[pml4t_offset].PageFrameNumber = ((uintptr_t)new_page) >> EFI_PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    pdpt = (HARDWARE_PTE*)((uintptr_t)pml4t[pml4t_offset].PageFrameNumber << EFI_PAGE_SHIFT);

    pdpt_offset = (va >> 30) & 0x1ff;

    if (!pdpt[pdpt_offset].Valid) {
        EFI_PHYSICAL_ADDRESS new_page;

        status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &new_page);
        if (EFI_ERROR(status)) {
            print_error(u"AllocatePages", status);
            return status;
        }

        memset((void*)new_page, 0, EFI_PAGE_SIZE);

        pdpt[pdpt_offset].Valid = 1;
        pdpt[pdpt_offset].Write = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdpt[pdpt_offset].PageFrameNumber = ((uintptr_t)new_page) >> EFI_PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    pdt = (HARDWARE_PTE*)((uintptr_t)pdpt[pdpt_offset].PageFrameNumber << EFI_PAGE_SHIFT);

    pdt_offset = (va >> 21) & 0x1ff;

    if (!pdt[pdt_offset].Valid) {
        EFI_PHYSICAL_ADDRESS new_page;

        status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &new_page);
        if (EFI_ERROR(status)) {
            print_error(u"AllocatePages", status);
            return status;
        }

        memset((void*)new_page, 0, EFI_PAGE_SIZE);

        pdt[pdt_offset].Valid = 1;
        pdt[pdt_offset].Write = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pdt[pdt_offset].PageFrameNumber = ((uintptr_t)new_page) >> EFI_PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    pt = (HARDWARE_PTE*)((uintptr_t)pdt[pdt_offset].PageFrameNumber << EFI_PAGE_SHIFT);

    pt_offset = (va >> EFI_PAGE_SHIFT) & 0x1ff;

    if (pt[pt_offset].Valid) {
        print(u"Mapping for ");
        print_hex(va);
        print(u" already set.\r\n");
        return EFI_INVALID_PARAMETER;
    }

    pt[pt_offset].Valid = 1;
    pt[pt_offset].Write = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pt[pt_offset].PageFrameNumber = pa >> EFI_PAGE_SHIFT;
#pragma GCC diagnostic pop

    return EFI_SUCCESS;
}

static EFI_STATUS do_map(uint64_t va, uint64_t pa, uint64_t len) {
    EFI_STATUS status;

    while (len > 0) {
        status = map_page(va, pa);

        if (EFI_ERROR(status))
            return status;

        va += EFI_PAGE_SIZE;
        pa += EFI_PAGE_SIZE;
        len -= EFI_PAGE_SIZE;
    }

    return EFI_SUCCESS;
}

EFI_STATUS setup_paging(uint64_t& cr3) {
    EFI_STATUS status;
    EFI_PHYSICAL_ADDRESS addr;
    map_entry* map_copy;
    unsigned int num_map_entries_copy;

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &addr);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePages", status);
        return status;
    }

    pml4t = (HARDWARE_PTE*)addr;

    num_map_entries_copy = num_map_entries;

    status = bs->AllocatePool(EfiLoaderData, sizeof(map_entry) * num_map_entries_copy, (void**)&map_copy);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return status;
    }

    memcpy(map_copy, map, sizeof(map_entry) * num_map_entries);

    memset(pml4t, 0, EFI_PAGE_SIZE);

    for (unsigned int i = 0; i < num_map_entries_copy; i++) {
        switch (map_copy[i].type) {
            case map_type::mem_bootloader:
            case map_type::mem_acpi:
                status = do_map(map_copy[i].addr, map_copy[i].addr, map_copy[i].len); // identity map

                if (EFI_ERROR(status))
                    return status;
                break;

            case map_type::mem_image:
                status = do_map(KERNEL_ADDR, map_copy[i].addr, map_copy[i].len);

                if (EFI_ERROR(status))
                    return status;
                break;

            case map_type::mem_stack:
                status = do_map(KERNEL_STACK, map_copy[i].addr, map_copy[i].len);

                if (EFI_ERROR(status))
                    return status;
                break;

            default:
                break;
        }
    }

    // add recursive mapping
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    pml4t[(SELFMAP & 0xff8000000000) >> 39].PageFrameNumber = (uintptr_t)pml4t >> EFI_PAGE_SHIFT;
#pragma GCC diagnostic pop
    pml4t[(SELFMAP & 0xff8000000000) >> 39].Valid = 1;
    pml4t[(SELFMAP & 0xff8000000000) >> 39].Write = 1;

    cr3 = (uintptr_t)pml4t;

    return EFI_SUCCESS;
}

static void remove_map_entry(int i) {
    memcpy(&map[i], &map[i+1], (num_map_entries - i - 1) * sizeof(map_entry));
    num_map_entries--;
}

EFI_STATUS add_map_entry(uint64_t start, uint64_t len, enum map_type type) {
    for (unsigned int i = 0; i < num_map_entries; i++) {
        if (map[i].addr >= start && map[i].addr + map[i].len <= start + len) { // remove whole entry
            remove_map_entry(i);
            i--;
        } else if (map[i].addr >= start && map[i].addr < start + len) { // remove start
            map[i].len = map[i].addr + map[i].len - start - len;
            map[i].addr = start + len;

            if (map[i].len == 0) {
                remove_map_entry(i);
                i--;
            }
        } else if (map[i].addr + map[i].len > start && map[i].addr + map[i].len <= start + len) { // remove end
            map[i].len = start - map[i].addr;

            if (map[i].len == 0) {
                remove_map_entry(i);
                i--;
            }
        } else if (start > map[i].addr && start + len < map[i].addr + map[i].len) { // remove middle
            if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
                print(u"Too many map entries.\r\n");
                return EFI_INVALID_PARAMETER;
            }

            insert_map_entry(i);
            num_map_entries++;

            map[i].len = start - map[i].addr;
            map[i+1].len = map[i+1].addr + map[i+1].len - start - len;
            map[i+1].addr = start + len;
        }
    }

    if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
        print(u"Too many map entries.\r\n");
        return EFI_INVALID_PARAMETER;
    }

    if (start == 0) {
        insert_map_entry(0);
        map[0].addr = 0;
        map[0].len = len;
        map[0].type = type;
        num_map_entries++;

        return EFI_SUCCESS;
    }

    for (unsigned int i = 0; i < num_map_entries; i++) {
        if (map[i].addr + map[i].len == start) {
            insert_map_entry(i+1);

            map[i+1].addr = start;
            map[i+1].len = len;
            map[i+1].type = type;

            num_map_entries++;
        }
    }

    return EFI_SUCCESS;
}
