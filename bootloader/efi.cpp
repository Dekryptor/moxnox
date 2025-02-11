#include <efi.h>
#include <string_view>
#include <span>
#include <pe.h>

using namespace std;

static EFI_SYSTEM_TABLE* systable;

static void print_string(string_view s) {
    wchar_t w[255], *t;

    // FIXME - make sure no overflow

    t = w;

    for (auto c : s) {
        if (c == '\n') {
            *t = '\r';
            t++;
        }

        *t = c;
        t++;
    }

    *t = 0;

    systable->ConOut->OutputString(systable->ConOut, (CHAR16*)w);
}

static const char* error_string(EFI_STATUS Status) {
    switch (Status) {
        case EFI_SUCCESS:
            return "EFI_SUCCESS";

        case EFI_LOAD_ERROR:
            return "EFI_LOAD_ERROR";

        case EFI_INVALID_PARAMETER:
            return "EFI_INVALID_PARAMETER";

        case EFI_UNSUPPORTED:
            return "EFI_UNSUPPORTED";

        case EFI_BAD_BUFFER_SIZE:
            return "EFI_BAD_BUFFER_SIZE";

        case EFI_BUFFER_TOO_SMALL:
            return "EFI_BUFFER_TOO_SMALL";

        case EFI_NOT_READY:
            return "EFI_NOT_READY";

        case EFI_DEVICE_ERROR:
            return "EFI_DEVICE_ERROR";

        case EFI_WRITE_PROTECTED:
            return "EFI_WRITE_PROTECTED";

        case EFI_OUT_OF_RESOURCES:
            return "EFI_OUT_OF_RESOURCES";

        case EFI_VOLUME_CORRUPTED:
            return "EFI_VOLUME_CORRUPTED";

        case EFI_VOLUME_FULL:
            return "EFI_VOLUME_FULL";

        case EFI_NO_MEDIA:
            return "EFI_NO_MEDIA";

        case EFI_MEDIA_CHANGED:
            return "EFI_MEDIA_CHANGED";

        case EFI_NOT_FOUND:
            return "EFI_NOT_FOUND";

        case EFI_ACCESS_DENIED:
            return "EFI_ACCESS_DENIED";

        case EFI_NO_RESPONSE:
            return "EFI_NO_RESPONSE";

        case EFI_NO_MAPPING:
            return "EFI_NO_MAPPING";

        case EFI_TIMEOUT:
            return "EFI_TIMEOUT";

        case EFI_NOT_STARTED:
            return "EFI_NOT_STARTED";

        case EFI_ALREADY_STARTED:
            return "EFI_ALREADY_STARTED";

        case EFI_ABORTED:
            return "EFI_ABORTED";

        case EFI_ICMP_ERROR:
            return "EFI_ICMP_ERROR";

        case EFI_TFTP_ERROR:
            return "EFI_TFTP_ERROR";

        case EFI_PROTOCOL_ERROR:
            return "EFI_PROTOCOL_ERROR";

        case EFI_INCOMPATIBLE_VERSION:
            return "EFI_INCOMPATIBLE_VERSION";

        case EFI_SECURITY_VIOLATION:
            return "EFI_SECURITY_VIOLATION";

        case EFI_CRC_ERROR:
            return "EFI_CRC_ERROR";

        case EFI_END_OF_MEDIA:
            return "EFI_END_OF_MEDIA";

        case EFI_END_OF_FILE:
            return "EFI_END_OF_FILE";

        case EFI_INVALID_LANGUAGE:
            return "EFI_INVALID_LANGUAGE";

        case EFI_COMPROMISED_DATA:
            return "EFI_COMPROMISED_DATA";

        default:
            return "(unknown error)";
    }
}

extern "C"
void* memcpy(void* dest, const void* src, size_t n) {
    auto orig_dest = dest;

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

    return orig_dest;
}

extern "C"
size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

static int memcmp(const void* s1, const void* s2, size_t n) {
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

static span<char> sp_strcpy(span<char> dest, string_view src) {
    auto to_copy = min(src.size(), dest.size());

    memcpy(dest.data(), src.data(), to_copy);

    return dest.subspan(to_copy);
}

static void print_error(const char* func, EFI_STATUS Status) {
    char s[255];
    auto sp = span(s, sizeof(s));

    sp = sp_strcpy(sp, func);
    sp = sp_strcpy(sp, " returned ");
    sp = sp_strcpy(sp, error_string(Status));
    sp = sp_strcpy(sp, "\n");

    print_string(string_view(s, sp.data() - s));
}

static span<char> sp_itoa_hex(span<char> dest, uint64_t num, unsigned int padding = 0) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0 && padding == 0) {
        dest[0] = '0';
        return dest.subspan(1);
    }

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || padding == 16 - i || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                dest[0] = (char)(n + '0');
            else
                dest[0] = (char)(n - 0xa + 'a');

            dest = dest.subspan(1);
            first = false;

            if (dest.empty())
                return dest;
        }

        num <<= 4;
    }

    return dest;
}

static EFI_STATUS get_image_information(EFI_BOOT_SERVICES* bs, EFI_HANDLE image_handle,
                                        EFI_LOADED_IMAGE_PROTOCOL*& image) {
    EFI_STATUS Status;
    EFI_GUID guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    Status = bs->OpenProtocol(image_handle, &guid, (void**)&image, image_handle, nullptr,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(Status)) {
        print_error("OpenProtocol", Status);
        return Status;
    }

    {
        char s[255];
        auto sp = span(s, sizeof(s));

        sp = sp_strcpy(sp, "EFI stub loaded at ");
        sp = sp_itoa_hex(sp, (uintptr_t)image->ImageBase);
        sp = sp_strcpy(sp, "\n");

        print_string(string_view(s, sp.data() - s));
    }

    return EFI_SUCCESS;
}

static span<uint8_t> find_payload(EFI_LOADED_IMAGE_PROTOCOL* image) {
    const auto& dos = *(IMAGE_DOS_HEADER*)image->ImageBase;
    const auto& pe = *(IMAGE_NT_HEADERS*)((uint8_t*)image->ImageBase + dos.e_lfanew);

    auto sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&pe.OptionalHeader64 + pe.FileHeader.SizeOfOptionalHeader),
                         pe.FileHeader.NumberOfSections);
    const IMAGE_SECTION_HEADER* payload_sect = nullptr;

    for (const auto& sect : sections) {
        if (!memcmp(sect.Name, ".payload", sizeof(sect.Name))) {
            payload_sect = &sect;
            break;
        }
    }

    if (!payload_sect) {
        print_string("Could not find .payload section.\n");
        return span<uint8_t>();
    }

    auto payload_size = *(uint64_t*)((uintptr_t)image->ImageBase + payload_sect->VirtualAddress);
    auto payload_data = (uint8_t*)((uintptr_t)image->ImageBase + payload_sect->VirtualAddress + sizeof(uint64_t));

    { // FIXME
        char s[255];
        auto sp = span(s, sizeof(s));

        sp = sp_strcpy(sp, "payload_size = ");
        sp = sp_itoa_hex(sp, payload_size);
        sp = sp_strcpy(sp, ", payload_data = ");
        sp = sp_itoa_hex(sp, (uintptr_t)payload_data);
        sp = sp_strcpy(sp, "\n");

        print_string(string_view(s, sp.data() - s));
    }

    return span(payload_data, payload_size);
}

extern "C"
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table) {
    UINTN key_event = 0;
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL* image;

    systable = system_table;

    Status = get_image_information(systable->BootServices, image_handle, image);
    if (EFI_ERROR(Status)) {
        print_error("get_image_information", Status);
        return Status;
    }

    auto payload = find_payload(image);
    if (payload.empty())
        return EFI_LOAD_ERROR;

    system_table->ConOut->OutputString(system_table->ConOut, (CHAR16*)L"Hello world!\r\n");

    system_table->BootServices->WaitForEvent(1, &system_table->ConIn->WaitForKey, &key_event);

    return EFI_SUCCESS;
}

