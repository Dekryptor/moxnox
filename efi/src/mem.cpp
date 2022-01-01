#include <stdint.h>
#include "efi.h"

enum class map_type { // FIXME - loaderblock.h
    mem_free,
    mem_reserved,
    mem_bootloader,
    mem_image,
    mem_paging_table,
    mem_stack,
    mem_acpi,
    mem_uefi_code,
    mem_uefi_data
};

struct map_entry { // FIXME - loaderblock.h
    uint64_t addr;
    uint64_t len;
    enum map_type type;
} __attribute__((packed));

static map_entry map[256];
static unsigned int num_map_entries = 0;

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

static void print_hex(uint64_t num) {
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
        case EfiACPIMemoryNVS:
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

    for (unsigned int i = 0; i < num_map_entries; i++) {
        const auto& me = map[i];

        print_hex(me.addr);
        print(u", ");
        print_hex(me.len);
        print(u", ");
        print_hex((uint64_t)me.type);
        print(u"; ");
    }

    print(u"\r\n");

    return EFI_SUCCESS;
}
