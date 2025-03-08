#include <efi.h>
#include <string_view>
#include <span>
#include <pe.h>
#include <loaderblock.h>

using namespace std;

static EFI_SYSTEM_TABLE* systable __attribute__((section (".data")));
static EFI_BOOT_SERVICES* bs __attribute__((section (".data")));

static const uintptr_t KERNEL_ADDR = 0xfffff80000000000;

static const uint64_t KERNEL_STACK = 0xfffff90000000000;
static const unsigned int KERNEL_STACK_PAGES = 4;

static map_entry map[256] __attribute__((section (".data")));
static unsigned int num_map_entries = 0;

static uint64_t debug_info_ptr __attribute__((section (".data")));
static uint64_t debug_abbrev_ptr __attribute__((section (".data")));
static uint64_t debug_rnglists_ptr __attribute__((section (".data")));
static uint64_t debug_str_ptr __attribute__((section (".data")));
static uint64_t debug_addr_ptr __attribute__((section (".data")));
static uint64_t debug_str_offsets_ptr __attribute__((section (".data")));
static uint32_t debug_info_size __attribute__((section (".data")));
static uint32_t debug_abbrev_size __attribute__((section (".data")));
static uint32_t debug_rnglists_size __attribute__((section (".data")));
static uint32_t debug_str_size __attribute__((section (".data")));
static uint32_t debug_addr_size __attribute__((section (".data")));
static uint32_t debug_str_offsets_size __attribute__((section (".data")));

static loader_block lb __attribute__((section (".data")));

// same size as on x86 Linux
static char cmdline[2048] __attribute__((section (".data")));

static uint8_t edid[128] __attribute__((section (".data")));
static bool have_edid __attribute__((section (".data"))) = false;

struct HARDWARE_PTE {
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
} __attribute__((packed));

static HARDWARE_PTE* pml4t __attribute__((section (".data")));

struct RSDP_DESCRIPTOR {
    char signature[8];
    uint8_t checksum;
    char oem_id [6];
    uint8_t revision;
    uint32_t rsdt_physical_address;
    uint32_t length;
    uint64_t xsdt_physical_address;
    uint8_t extended_checksum;
    char reserved[3];
} __attribute__((packed));

#define LINUX_EFI_INITRD_MEDIA_GUID {0x5568e427, 0x68fc, 0x4f3d, {0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68}}
#define EFI_LOAD_FILE2_PROTOCOL_GUID { 0x4006c0c1, 0xfcb3, 0x403e, { 0x99, 0x6d, 0x4a, 0x6c, 0x87, 0x24, 0xe0, 0x6d }}

static UINTN initrd_size = 0;
static EFI_PHYSICAL_ADDRESS initrd_data;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
}

static void print_string(string_view s) {
    wchar_t w[255], *t;

    // FIXME - make sure no overflow

    t = w;

    for (auto c : s) {
        if (c == '\n') {
            *t = '\r';
            t++;
        }

        *t = (wchar_t)c;
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

extern "C"
void* memset(void* s, int c, size_t n) {
    void* orig_s = s;
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

    print_string(string_view(s, (size_t)(sp.data() - s)));
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

static void utf16_to_utf8(span<char> buf, u16string_view u16sv) {
    if (buf.empty())
        return;

    for (unsigned int i = 0; i < u16sv.size(); i++) {
        uint32_t cp;

        if (u16sv[i] < 0xd800 || u16sv[i] > 0xdfff)
            cp = u16sv[i];
        else if (u16sv[i] >= 0xd800 && u16sv[i] < 0xdc00) {
            if (i == u16sv.size() - 1)
                return;

            auto next = u16sv[i+1];

            if (next < 0xdc00 || next > 0xdfff)
                return;

            cp = (((uint32_t)u16sv[i] & 0x3ff) << 10) | (next & 0x3ff);

            i++;
        } else // invalid
            return;

        if (cp < 0x80) {
            buf[0] = (char)cp;
            buf = buf.subspan(1);
        } else if (cp < 0x800) {
            if (buf.size() < 2)
                return;

            buf[0] = (char)(0xc0 | ((cp & 0x7c0) >> 6));
            buf[1] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(2);
        } else if (cp < 0x10000) {
            if (buf.size() < 3)
                return;

            buf[0] = (char)(0xe0 | ((cp & 0xf000) >> 12));
            buf[1] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[2] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(3);
        } else {
            if (buf.size() < 4)
                return;

            buf[0] = (char)(0xf0 | ((cp & 0xe0000) >> 18));
            buf[1] = (char)(0x80 | ((cp & 0x3f000) >> 12));
            buf[2] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[3] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(4);
        }
    }
}

static size_t utf16_to_utf8_len(u16string_view u16sv) {
    size_t len = 0;

    for (unsigned int i = 0; i < u16sv.size(); i++) {
        uint32_t cp;

        if (u16sv[i] < 0xd800 || u16sv[i] > 0xdfff)
            cp = u16sv[i];
        else if (u16sv[i] >= 0xd800 && u16sv[i] < 0xdc00) {
            if (i == u16sv.size() - 1)
                return 0;

            auto next = u16sv[i+1];

            if (next < 0xdc00 || next > 0xdfff)
                return 0;

            cp = (((uint32_t)u16sv[i] & 0x3ff) << 10) | (next & 0x3ff);

            i++;
        } else // invalid
            return 0;

        if (cp < 0x80)
            len++;
        else if (cp < 0x800)
            len += 2;
        else if (cp < 0x10000)
            len += 3;
        else
            len += 4;
    }

    return len;
}

static EFI_STATUS get_image_information(EFI_HANDLE image_handle,
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

        print_string(string_view(s, (size_t)(sp.data() - s)));
    }

    if (image->LoadOptionsSize == 0)
        cmdline[0] = 0;
    else {
        auto load_options = u16string_view((char16_t*)image->LoadOptions,
                                           image->LoadOptionsSize / sizeof(char16_t));

        while (!load_options.empty() && load_options.back() == 0) {
            load_options = u16string_view(load_options.data(),
                                          load_options.size() - 1);
        }

        utf16_to_utf8(cmdline, load_options);

        auto len = utf16_to_utf8_len(load_options);

        if (len >= sizeof(cmdline))
            cmdline[sizeof(cmdline) - 1] = 0;
        else
            cmdline[len] = 0;
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

    return span(payload_data, payload_size);
}

static bool check_header(span<const uint8_t> payload, IMAGE_NT_HEADERS** nth) {
    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)payload.data();
    IMAGE_NT_HEADERS* nt_header;

    if (payload.size() < sizeof(IMAGE_DOS_HEADER)) {
        print_string("Image was shorter than IMAGE_DOS_HEADER.\n");
        return false;
    }

    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        print_string("Incorrect DOS signature.\n");
        return false;
    }

    nt_header = (IMAGE_NT_HEADERS*)(payload.data() + dos_header->e_lfanew);

    // FIXME - check no overflow

    if (nt_header->Signature != IMAGE_NT_SIGNATURE) {
        print_string("Incorrect PE signature.\n");
        return false;
    }

    if (nt_header->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        print_string("Unsupported architecture.\n");
        return false;
    }

    // FIXME - make sure optional header size is at least minimum

    if (nt_header->OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt_header->OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        print_string("Unrecognized optional header signature.\n");
        return false;
    }

    // FIXME - check checksum

    // FIXME - check Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE

    *nth = nt_header;

    return true;
}

static bool do_relocations(uint8_t* data, uint64_t va, IMAGE_NT_HEADERS* nt_header) {
    IMAGE_BASE_RELOCATION* reloc;
    uint32_t size;
    uint16_t* addr;
    uint64_t base;

    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header->OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return true;
        }

        size = nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

        reloc = (IMAGE_BASE_RELOCATION*)(data + nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

        base = nt_header->OptionalHeader64.ImageBase;
    } else {
        if (nt_header->OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return true;
        }

        size = nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

        reloc = (IMAGE_BASE_RELOCATION*)(data + nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

        base = nt_header->OptionalHeader32.ImageBase;
    }

    do {
        uint32_t* ptr;

        if (size < reloc->SizeOfBlock)
            return true;

        // FIXME - check not out of bounds
        ptr = (uint32_t*)(data + reloc->VirtualAddress);

        addr = (uint16_t*)((uint8_t*)reloc + sizeof(IMAGE_BASE_RELOCATION));
        auto count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);

        for (unsigned int i = 0; i < count; i++) {
            uint16_t offset = addr[i] & 0xfff;
            uint16_t type = addr[i] >> 12;

            switch (type) {
                case IMAGE_REL_BASED_ABSOLUTE:
                    // nop
                break;

                case IMAGE_REL_BASED_HIGHLOW:
                {
                    uint32_t* ptr2 = (uint32_t*)((uint8_t*)ptr + offset);

                    *ptr2 = (uint32_t)(*ptr2 - base + va);
                    break;
                }

                case IMAGE_REL_BASED_DIR64:
                {
                    uint64_t* ptr2 = (uint64_t*)((uint8_t*)ptr + offset);

                    *ptr2 = *ptr2 - base + va;
                    break;
                }

                default:
                    print_string("Unsupported relocation type.\n"); // FIXME - include number

                    return false;
            }
        }

        size -= reloc->SizeOfBlock;

        if (size < sizeof(IMAGE_BASE_RELOCATION))
            return true;

        reloc = (IMAGE_BASE_RELOCATION*)((uint8_t*)reloc + reloc->SizeOfBlock);
    } while (true);

    return true;
}

static string_view get_section_name(const IMAGE_SECTION_HEADER& s, const IMAGE_FILE_HEADER& file_header,
                                    span<const uint8_t> payload) {
    auto name = string_view(s.Name, sizeof(s.Name));

    while (!name.empty() && name.back() == 0) {
        name.remove_suffix(1);
    }

    if (name.empty() || name[0] != '/' || file_header.PointerToSymbolTable == 0)
        return name;

    // decode long section names

    unsigned int num = 0;
    uint32_t string_table = file_header.PointerToSymbolTable + (file_header.NumberOfSymbols * 18);

    for (auto c : string_view(name.data() + 1, name.size() - 1)) {
        num *= 10;

        if (c >= '0' && c <= '9')
            num += (unsigned int)(c - '0');
        else
            return name;
    }

    return string_view((char*)payload.data() + string_table + num);
}

EFI_STATUS load_pe(span<const uint8_t> payload, uint64_t va, uint64_t& entry_point, uint64_t& img_pa, size_t& img_size) {
    EFI_STATUS status;
    IMAGE_NT_HEADERS* nt_header;
    size_t size;
    EFI_PHYSICAL_ADDRESS pa;

    if (!check_header(payload, &nt_header))
        return EFI_INVALID_PARAMETER;

    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        size = nt_header->OptionalHeader64.SizeOfImage;
    else
        size = nt_header->OptionalHeader32.SizeOfImage;

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, bytes_to_pages(size), &pa);
    if (EFI_ERROR(status)) {
        print_string("Could not find enough bytes free for kernel image.\n");
        print_error("AllocatePages", status);
        return status;
    }

    auto addr = (uint8_t*)pa;

    // copy header
    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        memcpy(addr, payload.data(), nt_header->OptionalHeader64.SizeOfHeaders);
    else
        memcpy(addr, payload.data(), nt_header->OptionalHeader32.SizeOfHeaders);

    span sects((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header->OptionalHeader64 + nt_header->FileHeader.SizeOfOptionalHeader),
               nt_header->FileHeader.NumberOfSections);

    for (const auto& s : sects) {
        uint32_t section_size;
        auto name = get_section_name(s, nt_header->FileHeader, payload);

        // FIXME - check no overruns

        if (name == ".debug_info") {
            debug_info_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_info_size = s.VirtualSize;
        } else if (name == ".debug_abbrev") {
            debug_abbrev_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_abbrev_size = s.VirtualSize;
        } else if (name == ".debug_rnglists") {
            debug_rnglists_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_rnglists_size = s.VirtualSize;
        } else if (name == ".debug_str") {
            debug_str_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_str_size = s.VirtualSize;
        } else if (name == ".debug_addr") {
            debug_addr_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_addr_size = s.VirtualSize;
        } else if (name == ".debug_str_offsets") {
            debug_str_offsets_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_str_offsets_size = s.VirtualSize;
        }

        section_size = s.VirtualSize;

        if (s.SizeOfRawData < section_size)
            section_size = s.SizeOfRawData;

        if (section_size > 0 && s.PointerToRawData != 0)
            memcpy(addr + s.VirtualAddress, payload.data() + s.PointerToRawData, section_size);

        if (section_size < s.VirtualSize) // if short, pad with zeroes
            memset(addr + s.VirtualAddress + section_size, 0, s.VirtualSize - section_size);
    }

    if (!do_relocations(addr, va, nt_header))
        return EFI_INVALID_PARAMETER;

    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        entry_point = nt_header->OptionalHeader64.AddressOfEntryPoint;
    else
        entry_point = nt_header->OptionalHeader32.AddressOfEntryPoint;

    entry_point += va;
    img_pa = pa;
    img_size = size;

    return EFI_SUCCESS;
}

static void insert_map_entry(unsigned int i) {
    // FIXME - memmove?
    for (int j = (int)num_map_entries - 1; j >= (int)i; j--) {
        memcpy(&map[j+1], &map[j], sizeof(map_entry));
    }
}

static void remove_map_entry(unsigned int i) {
    memcpy(&map[i], &map[i+1], (size_t)(num_map_entries - i - 1) * sizeof(map_entry));
    num_map_entries--;
}

static EFI_STATUS add_map_entry(uint64_t start, uint64_t len, enum mem_type type) {
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
                print_string("Too many map entries.\n");
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
        print_string("Too many map entries.\n");
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

static enum mem_type map_efi_type(EFI_MEMORY_TYPE Type) {
    switch (Type) {
        case EfiReservedMemoryType:
        case EfiUnusableMemory:
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
        case EfiPalCode:
            return mem_type::reserved;

        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
            return mem_type::bootloader;

        case EfiRuntimeServicesCode:
            return mem_type::uefi_code;

        case EfiRuntimeServicesData:
            return mem_type::uefi_data;

        case EfiConventionalMemory:
            return mem_type::free;

        case EfiACPIMemoryNVS:
        case EfiACPIReclaimMemory:
            return mem_type::acpi;

        default:
            return mem_type::reserved;
    }
}

static EFI_STATUS get_memory_map() {
    EFI_STATUS status;
    UINTN map_size, key, descriptor_size;
    UINT32 descriptor_version;
    EFI_MEMORY_DESCRIPTOR* memory_map = nullptr;

    map_size = 0;

    status = bs->GetMemoryMap(&map_size, memory_map, &key, &descriptor_size, &descriptor_version);
    if (status == EFI_SUCCESS) {
        print_string("GetMemoryMap did not return memory map.\r\n");
        return EFI_INVALID_PARAMETER;
    } else if (status != EFI_BUFFER_TOO_SMALL) {
        print_error("GetMemoryMap", status);
        return status;
    }

    map_size *= 2; // some leeway, because AllocatePool can change things

    status = bs->AllocatePool(EfiBootServicesData, map_size, (void**)&memory_map);
    if (EFI_ERROR(status)) {
        print_error("AllocatePool", status);
        return status;
    }

    status = bs->GetMemoryMap(&map_size, memory_map, &key, &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        print_error("GetMemoryMap", status);
        bs->FreePool(memory_map);
        return status;
    }

    auto ptr = (uint8_t*)memory_map;
    auto left = map_size;

    while (left > descriptor_size) {
        auto& m = *(EFI_MEMORY_DESCRIPTOR*)ptr;
        auto& me = map[num_map_entries];

        if (num_map_entries == sizeof(map) / sizeof(map[0])) {
            print_string("Too many entries in memory map.\n");
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
            print_string("Too many entries in memory map.\n");
            return EFI_INVALID_PARAMETER;
        }

        insert_map_entry(0);

        map[0].addr = 0;
        map[0].len = map[1].addr;
        map[0].type = mem_type::reserved;
    } else {
        if (map[0].type != mem_type::reserved) { // snatch first page
            insert_map_entry(0);

            map[0].addr = 0;
            map[0].len = EFI_PAGE_SIZE;
            map[0].type = mem_type::reserved;

            map[1].addr += EFI_PAGE_SIZE;
            map[1].len -= EFI_PAGE_SIZE;
        }
    }

    for (unsigned int i = 0; i < num_map_entries - 1; i++) {
        if (map[i].addr + map[i].len < map[i+1].addr) {
            if (num_map_entries == sizeof(map) / sizeof(map_entry)) {
                print_string("Too many entries in memory map.\n");
                return EFI_INVALID_PARAMETER;
            }

            insert_map_entry(i+1);

            map[i+1].addr = map[i].addr + map[i].len;
            map[i+1].len = map[i+2].addr - map[i+1].addr;
            map[i+1].type = mem_type::reserved;

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
    if (map[num_map_entries - 1].type == mem_type::reserved)
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
            print_error("AllocatePages", status);
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
            print_error("AllocatePages", status);
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
            print_error("AllocatePages", status);
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
        char s[255];
        auto sp = span(s, sizeof(s));

        sp = sp_strcpy(sp, "Mapping for ");
        sp = sp_itoa_hex(sp, va);
        sp = sp_strcpy(sp, " already set.\n");

        print_string(string_view(s, (size_t)(sp.data() - s)));
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

static EFI_STATUS setup_paging(uint64_t& cr3) {
    EFI_STATUS status;
    EFI_PHYSICAL_ADDRESS addr;
    map_entry* map_copy;
    unsigned int num_map_entries_copy;

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &addr);
    if (EFI_ERROR(status)) {
        print_error("AllocatePages", status);
        return status;
    }

    pml4t = (HARDWARE_PTE*)addr;

    num_map_entries_copy = num_map_entries;

    status = bs->AllocatePool(EfiLoaderData, sizeof(map_entry) * num_map_entries_copy, (void**)&map_copy);
    if (EFI_ERROR(status)) {
        print_error("AllocatePool", status);
        return status;
    }

    memcpy(map_copy, map, sizeof(map_entry) * num_map_entries);

    memset(pml4t, 0, EFI_PAGE_SIZE);

    for (unsigned int i = 0; i < num_map_entries_copy; i++) {
        switch (map_copy[i].type) {
            case mem_type::bootloader:
            case mem_type::acpi:
            case mem_type::initrd:
                status = do_map(map_copy[i].addr, map_copy[i].addr, map_copy[i].len); // identity map

                if (EFI_ERROR(status))
                    return status;
                break;

            case mem_type::image:
                status = do_map(KERNEL_ADDR, map_copy[i].addr, map_copy[i].len);

                if (EFI_ERROR(status))
                    return status;
                break;

            case mem_type::stack:
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

static EFI_STATUS find_acpi_tables(uint64_t& addr) {
    EFI_GUID acpi2_guid = ACPI_20_TABLE_GUID;
    RSDP_DESCRIPTOR* rsdp = nullptr;

    for (unsigned int i = 0; i < systable->NumberOfTableEntries; i++) {
        if (!memcmp(&systable->ConfigurationTable[i].VendorGuid, &acpi2_guid, sizeof(EFI_GUID))) {
            rsdp = (RSDP_DESCRIPTOR*)systable->ConfigurationTable[i].VendorTable;
            break;
        }
    }

    if (!rsdp) {
        print_string("ACPI 2.0 tables not found\n");
        return EFI_NOT_FOUND;
    }

    if (rsdp->revision == 2)
        addr = rsdp->xsdt_physical_address;
    else {
        char s[255];
        auto sp = span(s, sizeof(s));

        sp = sp_strcpy(sp, "Unrecognized ACPI revision ");
        sp = sp_itoa_hex(sp, rsdp->revision);
        sp = sp_strcpy(sp, "\n");

        print_string(string_view(s, (size_t)(sp.data() - s)));

        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

static const struct {
    VENDOR_DEVICE_PATH vendor;
    EFI_DEVICE_PATH_PROTOCOL end;
} __attribute__((packed)) initrd_dev_path = {
    {
        {
            MEDIA_DEVICE_PATH,
            MEDIA_VENDOR_DP,
            { sizeof(VENDOR_DEVICE_PATH), 0 }
        },
        LINUX_EFI_INITRD_MEDIA_GUID
    },
    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        { sizeof(EFI_DEVICE_PATH_PROTOCOL), 0 }
    }
};

static EFI_STATUS find_initrd() {
    EFI_STATUS Status;
    EFI_GUID load_file2_guid = EFI_LOAD_FILE2_PROTOCOL_GUID;
    auto devpath = (EFI_DEVICE_PATH_PROTOCOL*)&initrd_dev_path;
    EFI_HANDLE handle;
    EFI_LOAD_FILE_PROTOCOL* lf2;

    Status = bs->LocateDevicePath(&load_file2_guid, &devpath, &handle);
    if (EFI_ERROR(Status))
        return Status;

    Status = bs->HandleProtocol(handle, &load_file2_guid, (void**)&lf2);
    if (EFI_ERROR(Status))
        return Status;

    Status = lf2->LoadFile(lf2, devpath, false, &initrd_size, nullptr);
    if (Status != EFI_BUFFER_TOO_SMALL)
        return Status;

    if (initrd_size == 0)
        return EFI_NOT_FOUND;

    Status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData,
                               bytes_to_pages(initrd_size), &initrd_data);
    if (EFI_ERROR(Status))
        return Status;

    Status = lf2->LoadFile(lf2, devpath, false, &initrd_size, (void*)initrd_data);
    if (EFI_ERROR(Status))
        return Status;

    return EFI_SUCCESS;
}

__attribute__((always_inline))
static uint64_t inline __readcr0() {
    uint64_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr0"
        : "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
static void inline __writecr0(uint64_t v) {
    __asm__ __volatile__ (
        "mov\tcr0, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
static void inline __writecr3(uint64_t v) {
    __asm__ __volatile__ (
        "mov\tcr3, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
static uint64_t inline __readcr4() {
    uint64_t ret;

    __asm__ __volatile__ (
        "mov\t%0, cr4"
        : "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
static void inline __writecr4(uint64_t v) {
    __asm__ __volatile__ (
        "mov\tcr4, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
static uint64_t inline __readmsr(uint32_t r) {
    uint32_t high, low;

    __asm__ __volatile__ (
        "rdmsr"
        : "=d" (high), "=a" (low)
        : "c" (r)
    );

    return ((uint64_t)high << 32) | low;
}

__attribute__((always_inline))
static void inline __writemsr(uint32_t r, uint64_t v) {
    __asm__ __volatile__ (
        "wrmsr"
        :
        : "c" (r), "d" (v >> 32), "a" (v & 0xffffffff)
    );
}

__attribute__((always_inline))
static void inline _disable() {
    __asm__ __volatile__ (
        "cli"
    );
}

__attribute__((noreturn))
static void start_kernel(uint64_t entry_point, uint64_t pml4t, loader_block* lb) {
    _disable();

    // change to virtual addresses
    __writecr3(pml4t);

    // FIXME - GDT?

    // set MP and WP bits on CR0
    __writecr0(__readcr0() | 0x10002);

    // set OSFXSR and OSXMMEXCPT flags on CR4
    __writecr4(__readcr4() | 0x600);

    // enable NX bit and System Call Extensions
    __writemsr(0xc0000080, __readmsr(0xc0000080) | 0x801);

    // set stack and jump to entry point
    __asm__ __volatile__ (
        "mov\trsp, %0"
        "\n\tjmp\t%2"
        :
        : "i" (KERNEL_STACK + (KERNEL_STACK_PAGES << EFI_PAGE_SHIFT) - 0x20), // 0x20 for MS ABI
          "c" (lb),
          "r" (entry_point)
    );

    __builtin_unreachable();
}

static EFI_STATUS go(EFI_HANDLE image_handle, uint64_t entry_point, uint64_t cr3, uint64_t acpi_tables) {
    EFI_STATUS status;
    UINTN size, key, descsize;
    UINT32 version;
    EFI_MEMORY_DESCRIPTOR* mapdesc;

    lb.map_ptr = (uintptr_t)map;
    lb.num_map_entries = num_map_entries;
    lb.rsdt_pa = acpi_tables;
    lb.text_mode = false;
    lb.cmdline = (uintptr_t)cmdline;
    lb.initrd_data = initrd_data;
    lb.initrd_size = initrd_size;
    lb.edid = have_edid ? (uintptr_t)edid : 0;
    lb.debug_info_ptr = debug_info_ptr;
    lb.debug_info_size = debug_info_size;
    lb.debug_abbrev_ptr = debug_abbrev_ptr;
    lb.debug_abbrev_size = debug_abbrev_size;
    lb.debug_rnglists_ptr = debug_rnglists_ptr;
    lb.debug_rnglists_size = debug_rnglists_size;
    lb.debug_str_ptr = debug_str_ptr;
    lb.debug_str_size = debug_str_size;
    lb.debug_addr_ptr = debug_addr_ptr;
    lb.debug_addr_size = debug_addr_size;
    lb.debug_str_offsets_ptr = debug_str_offsets_ptr;
    lb.debug_str_offsets_size = debug_str_offsets_size;

    // exit boot services

    size = 0;

    // get new key
    status = bs->GetMemoryMap(&size, nullptr, &key, &descsize, &version);
    if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
        print_error("GetMemoryMap", status);
        return status;
    }

    size *= 2;

    status = bs->AllocatePool(EfiLoaderData, size, (void**)&mapdesc);
    if (EFI_ERROR(status)) {
        print_error("AllocatePool", status);
        return status;
    }

    status = bs->GetMemoryMap(&size, mapdesc, &key, &descsize, &version);
    if (EFI_ERROR(status)) {
        print_error("GetMemoryMap", status);
        return status;
    }

    status = bs->ExitBootServices(image_handle, key);
    if (EFI_ERROR(status)) {
        print_error("ExitBootServices", status);
        return status;
    }

    // FIXME - SetVirtualAddressMap

    start_kernel(entry_point, cr3, &lb);

    return EFI_SUCCESS; // never reached
}

static char* strstr(const char* haystack, const char* needle) {
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

static void parse_video_option(unsigned int& preferred_width,
                               unsigned int& preferred_height) {
    if (cmdline[0] == 0)
        return;

    static const char param[] = "video=";

    auto ptr = strstr(cmdline, param);
    if (!ptr)
        return;

    ptr += sizeof(param) - 1;

    auto sv = string_view(ptr);

    for (size_t i = 0; i < sv.size(); i++) {
        if (sv[i] == ' ') {
            sv = string_view(sv.data(), i);
            break;
        }
    }

    preferred_width = preferred_height = 0;

    while (!sv.empty()) {
        if (sv.front() == 'x')
            break;
        else if (sv.front() >= '0' && sv.front() <= '9') {
            preferred_width *= 10;
            preferred_width += (unsigned int)(sv.front() - '0');
        } else
            break;

        sv = string_view(sv.data() + 1, sv.size() - 1);
    }

    if (sv.empty() || sv.front() != 'x') {
        preferred_width = 0;
        return;
    }

    sv = string_view(sv.data() + 1, sv.size() - 1);

    while (!sv.empty()) {
        if (sv.front() >= '0' && sv.front() <= '9') {
            preferred_height *= 10;
            preferred_height += (unsigned int)(sv.front() - '0');
        } else {
            preferred_width = preferred_height = 0;
            return;
        }

        sv = string_view(sv.data() + 1, sv.size() - 1);
    }
}

static EFI_STATUS init_framebuffer(EFI_HANDLE image_handle) {
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GUID edid_guid = EFI_EDID_DISCOVERED_PROTOCOL_GUID;
    EFI_HANDLE* handles = NULL;
    UINTN count;
    EFI_STATUS status;
    unsigned int preferred_width = 0, preferred_height = 0;

    parse_video_option(preferred_width, preferred_height);

    status = bs->LocateHandleBuffer(ByProtocol, &guid, NULL, &count, &handles);
    if (EFI_ERROR(status))
        return status;

    for (unsigned int i = 0; i < count; i++) {
        EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;
        EFI_EDID_DISCOVERED_PROTOCOL* edp = nullptr;
        unsigned int mode = 0;
        unsigned int pixels = 0;

        status = bs->OpenProtocol(handles[i], &guid, (void**)&gop, image_handle,
                                  nullptr, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (EFI_ERROR(status)) {
            print_error("OpenProtocol", status);
            continue;
        }

        status = bs->OpenProtocol(handles[i], &edid_guid, (void**)&edp, image_handle,
                                  nullptr, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (!EFI_ERROR(status)) {
            if (edp && edp->SizeOfEdid >= 128) {
                memcpy(edid, edp->Edid, sizeof(edid));
                have_edid = true;
            }

            bs->CloseProtocol(handles[i], &edid_guid, image_handle, nullptr);
        }

        if (have_edid && preferred_width == 0 && preferred_height == 0) {
            preferred_width = (uint16_t)((edid[0x3a] & 0xf0) << 4) | edid[0x38];
            preferred_height = (uint16_t)((edid[0x3d] & 0xf0) << 4) | edid[0x3b];
        }

        for (unsigned int j = 0; j < gop->Mode->MaxMode; j++) {
            UINTN size;
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;

            status = gop->QueryMode(gop, j, &size, &info);
            if (EFI_ERROR(status)) {
                print_error("QueryMode", status);
                continue;
            }

            // choose the best mode

            if (info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor)
                continue;

            if (info->HorizontalResolution == 0 || info->VerticalResolution == 0)
                continue;

            if (info->HorizontalResolution == preferred_width &&
                info->VerticalResolution == preferred_height) {
                mode = j;
                break;
            }

            if (info->HorizontalResolution * info->VerticalResolution > pixels) {
                mode = j;
                pixels = info->HorizontalResolution * info->VerticalResolution;
            }
        }

        status = gop->SetMode(gop, mode);
        if (EFI_ERROR(status)) {
            print_error("SetMode", status);
            bs->CloseProtocol(handles[i], &guid, image_handle, nullptr);
            goto end;
        }

        lb.framebuffer.phys_address = (uintptr_t)gop->Mode->FrameBufferBase;
        lb.framebuffer.width = gop->Mode->Info->HorizontalResolution;
        lb.framebuffer.height = gop->Mode->Info->VerticalResolution;
        lb.framebuffer.pixels_per_scan_line = gop->Mode->Info->PixelsPerScanLine;

        bs->CloseProtocol(handles[i], &guid, image_handle, nullptr);

        status = EFI_SUCCESS;
        goto end;
    }

    status = EFI_NOT_FOUND;

end:
    bs->FreePool(handles);

    return status;
}

extern "C"
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL* image;
    uint64_t entry_point, cr3, img_pa, acpi_tables;
    size_t img_size;
    EFI_PHYSICAL_ADDRESS stack;

    systable = system_table;
    bs = system_table->BootServices;

    Status = get_image_information(image_handle, image);
    if (EFI_ERROR(Status)) {
        print_error("get_image_information", Status);
        return Status;
    }

    auto payload = find_payload(image);
    if (payload.empty())
        return EFI_LOAD_ERROR;

    Status = load_pe(payload, KERNEL_ADDR, entry_point, img_pa, img_size);
    if (EFI_ERROR(Status)) {
        print_error("load_pe", Status);
        return Status;
    }

    Status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, KERNEL_STACK_PAGES, &stack);
    if (EFI_ERROR(Status)) {
        print_error("AllocatePages", Status);
        return Status;
    }

    Status = find_acpi_tables(acpi_tables);
    if (EFI_ERROR(Status)) {
        print_error("find_acpi_tables", Status);
        return Status;
    }

    Status = find_initrd();
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
        print_error("find_initrd", Status);
        return Status;
    }

    Status = init_framebuffer(image_handle);
    if (EFI_ERROR(Status)) {
        print_error("init_framebuffer", Status);
        return Status;
    }

    Status = get_memory_map();
    if (EFI_ERROR(Status)) {
        print_error("get_memory_map", Status);
        return Status;
    }

    Status = add_map_entry(img_pa, img_size, mem_type::image);
    if (EFI_ERROR(Status)) {
        print_error("add_map_entry", Status);
        return Status;
    }

    Status = add_map_entry(stack, KERNEL_STACK_PAGES << EFI_PAGE_SHIFT, mem_type::stack);
    if (EFI_ERROR(Status)) {
        print_error("add_map_entry", Status);
        return Status;
    }

    if (initrd_size != 0) {
        Status = add_map_entry(initrd_data, bytes_to_pages(initrd_size) << EFI_PAGE_SHIFT,
                               mem_type::initrd);
        if (EFI_ERROR(Status)) {
            print_error("add_map_entry", Status);
            return Status;
        }
    }

    Status = add_map_entry((uintptr_t)lb.framebuffer.phys_address,
                           bytes_to_pages(lb.framebuffer.pixels_per_scan_line * lb.framebuffer.height * 4) << EFI_PAGE_SHIFT,
                           mem_type::framebuffer);
    if (EFI_ERROR(Status)) {
        print_error("add_map_entry", Status);
        return Status;
    }

    Status = setup_paging(cr3);
    if (EFI_ERROR(Status)) {
        print_error("setup_paging", Status);
        return Status;
    }

    // FIXME - pass runtime services list through to kernel

    Status = go(image_handle, entry_point, cr3, acpi_tables); // doesn't return unless unsuccessful
    print_error("go", Status);

    return Status;
}
