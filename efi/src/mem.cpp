#include <stdint.h>
#include "efi.h"

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

EFI_STATUS get_memory_map() {
    EFI_STATUS status;
    UINTN map_size, key, descriptor_size;
    UINT32 descriptor_version;
    EFI_MEMORY_DESCRIPTOR* map = nullptr;

    map_size = 0;

    status = bs->GetMemoryMap(&map_size, map, &key, &descriptor_size, &descriptor_version);
    if (status == EFI_SUCCESS) {
        print(u"GetMemoryMap did not return memory map.\r\n");
        return EFI_INVALID_PARAMETER;
    } else if (status != EFI_BUFFER_TOO_SMALL) {
        print_error(u"GetMemoryMap", status);
        return status;
    }

    map_size *= 2; // some leeway, because AllocatePool can change things

    status = bs->AllocatePool(EfiBootServicesData, map_size, (void**)&map);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return status;
    }

    status = bs->GetMemoryMap(&map_size, map, &key, &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        print_error(u"GetMemoryMap", status);
        bs->FreePool(map);
        return status;
    }

    auto ptr = (uint8_t*)map;
    auto left = map_size;

    while (left > descriptor_size) {
        auto& m = *(EFI_MEMORY_DESCRIPTOR*)ptr;

        // FIXME - map EFI type to our type
        // FIXME - add to mapping array

        print_hex(m.Type);
        print(u", ");
        print_hex(m.PhysicalStart);
        print(u", ");
        print_hex(m.NumberOfPages);
        print(u"; ");

        if (left == descriptor_size)
            break;

        ptr += descriptor_size;
        left -= descriptor_size;
    }

    print(u"\r\n");

    // FIXME - add missing entries to mapping array
    // FIXME - merge entries with same type
    // FIXME - remove final entry if inaccessible

    bs->FreePool(map);

    return EFI_SUCCESS;
}
