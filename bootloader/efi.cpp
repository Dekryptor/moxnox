#include <efi.h>
#include <string_view>
#include <span>
#include <tuple>
#include <algorithm>
#include <string.h>

using namespace std;

import format;
import pe;
import loaderblock;

static EFI_SYSTEM_TABLE* systable __attribute__((section (".data")));
static EFI_BOOT_SERVICES* bs __attribute__((section (".data")));

static const uintptr_t KERNEL_ADDR = 0xfffff80000000000;

static map_entry map[256] __attribute__((section (".data")));
static unsigned int num_map_entries = 0;

static uint64_t debug_info_ptr __attribute__((section (".data")));
static uint64_t debug_abbrev_ptr __attribute__((section (".data")));
static uint64_t debug_rnglists_ptr __attribute__((section (".data")));
static uint64_t debug_str_ptr __attribute__((section (".data")));
static uint64_t debug_addr_ptr __attribute__((section (".data")));
static uint64_t debug_str_offsets_ptr __attribute__((section (".data")));
static uint64_t debug_line_ptr __attribute__((section (".data")));
static uint64_t debug_line_str_ptr __attribute__((section (".data")));
static uint32_t debug_info_size __attribute__((section (".data")));
static uint32_t debug_abbrev_size __attribute__((section (".data")));
static uint32_t debug_rnglists_size __attribute__((section (".data")));
static uint32_t debug_str_size __attribute__((section (".data")));
static uint32_t debug_addr_size __attribute__((section (".data")));
static uint32_t debug_str_offsets_size __attribute__((section (".data")));
static uint32_t debug_line_size __attribute__((section (".data")));
static uint32_t debug_line_str_size __attribute__((section (".data")));

static loader_block lb __attribute__((section (".data")));

// same size as on x86 Linux
static char cmdline[2048] __attribute__((section (".data")));
static u16string_view wcmdline __attribute__((section (".data")));

static uint8_t edid[128] __attribute__((section (".data")));
static bool have_edid __attribute__((section (".data"))) = false;

#ifdef __aarch64__

union ARM64_TCR_EL1 {
    struct {
        uint64_t T0SZ : 6;
        uint64_t : 1;
        uint64_t EPD0 : 1;
        uint64_t IRGN0 : 2;
        uint64_t ORGN0 : 2;
        uint64_t SH0 : 2;
        uint64_t TG0 : 2;
        uint64_t T1SZ : 6;
        uint64_t A1 : 1;
        uint64_t EPD1 : 1;
        uint64_t IRGN1 : 2;
        uint64_t ORGN1 : 2;
        uint64_t SH1 : 2;
        uint64_t TG1 : 2;
        uint64_t IPS : 3;
        uint64_t : 1;
        uint64_t AS : 1;
        uint64_t TBI0 : 1;
        uint64_t TBI1 : 1;
        uint64_t HA : 1;
        uint64_t HD : 1;
        uint64_t HPD0 : 1;
        uint64_t HPD1 : 1;
        uint64_t HWU059 : 1;
        uint64_t HWU060 : 1;
        uint64_t HWU061 : 1;
        uint64_t HWU062 : 1;
        uint64_t HWU159 : 1;
        uint64_t HWU160 : 1;
        uint64_t HWU161 : 1;
        uint64_t HWU162 : 1;
        uint64_t TBID0 : 1;
        uint64_t TBID1 : 1;
        uint64_t NFD0 : 1;
        uint64_t NFD1 : 1;
        uint64_t E0PD0 : 1;
        uint64_t E0PD1 : 1;
        uint64_t TCMA0 : 1;
        uint64_t TCMA1 : 1;
        uint64_t DS : 1;
        uint64_t MTX0 : 1;
        uint64_t MTX1 : 1;
        uint64_t : 2;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_TCR_EL1) == 8);

union ARM64_SCTLR_EL1 {
    struct {
        uint64_t M : 1;
        uint64_t A : 1;
        uint64_t C : 1;
        uint64_t SA : 1;
        uint64_t SA0 : 1;
        uint64_t : 1;
        uint64_t nAA : 1;
        uint64_t : 2;
        uint64_t UMA : 1;
        uint64_t EnRCTX : 1;
        uint64_t : 1;
        uint64_t I : 1;
        uint64_t EnDB : 1;
        uint64_t DZE : 1;
        uint64_t UCT : 1;
        uint64_t nTWI : 1;
        uint64_t BR : 1;
        uint64_t nTWE : 1;
        uint64_t WXN : 1;
        uint64_t : 1;
        uint64_t IESB : 1;
        uint64_t : 1;
        uint64_t SPAN : 1;
        uint64_t E0E : 1;
        uint64_t EE : 1;
        uint64_t UCI : 1;
        uint64_t EnDA : 1;
        uint64_t : 2;
        uint64_t EnIB : 1;
        uint64_t EnIA : 1;
        uint64_t : 12;
        uint64_t DSSBS : 1;
        uint64_t : 19;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_SCTLR_EL1) == 8);

struct ARM64_HARDWARE_PTE {
    uint64_t valid : 1;
    uint64_t table_entry : 1;
    uint64_t AttrIndex : 3;
    uint64_t NS : 1;
    uint64_t AP : 2;
    uint64_t SH : 2;
    uint64_t AF : 1;
    uint64_t nG : 1;
    uint64_t pfn : 39;
    uint64_t DBM : 1;
    uint64_t Contig : 1;
    uint64_t PXN : 1;
    uint64_t UXN : 1;
    uint64_t software : 4;
    uint64_t imp_def : 4;
    uint64_t : 1;
} __attribute((packed));

static_assert(sizeof(ARM64_HARDWARE_PTE) == 8);

static ARM64_HARDWARE_PTE* ttbr0 __attribute__((section (".data")));
static ARM64_HARDWARE_PTE* ttbr1 __attribute__((section (".data")));

#else

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

#endif

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

template<typename... Args>
static void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char s[100];
    auto sp = span(s, sizeof(s));

    sp = format_to(sp, fmt.sv, make_tuple(args...));

    print_string(string_view(s, (size_t)(sp.data() - s)));
}

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

static void print_error(const char* func, EFI_STATUS Status) {
    print("{} returned {}\n", func, error_string(Status));
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

    print("EFI stub loaded at {:x}\n", (uintptr_t)image->ImageBase);

    if (image->LoadOptionsSize == 0) {
        cmdline[0] = 0;
        wcmdline = u16string_view();
    } else {
        wcmdline = u16string_view((char16_t*)image->LoadOptions,
                                  image->LoadOptionsSize / sizeof(char16_t));

        while (!wcmdline.empty() && wcmdline.back() == 0) {
            wcmdline = u16string_view(wcmdline.data(), wcmdline.size() - 1);
        }

        utf16_to_utf8(cmdline, wcmdline);

        auto len = utf16_to_utf8_len(wcmdline);

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

    auto payload_size = payload_sect->VirtualSize;
    auto payload_data = (uint8_t*)((uintptr_t)image->ImageBase + payload_sect->VirtualAddress);

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

#ifdef __aarch64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_ARM64;
#else
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_AMD64;
#endif

    if (nt_header->FileHeader.Machine != this_arch) {
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
                    print("Unsupported relocation type {}.\n", type);
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
        } else if (name == ".debug_line") {
            debug_line_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_line_size = s.VirtualSize;
        } else if (name == ".debug_line_str") {
            debug_line_str_ptr = KERNEL_ADDR + s.VirtualAddress;
            debug_line_str_size = s.VirtualSize;
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
            if (map[i].type == type) {
                map[i].len += len;
                return EFI_SUCCESS;
            }

            if (i < num_map_entries - 1 && map[i+1].addr == start + len && map[i+1].type == type) {
                map[i+1].addr = start;
                map[i+1].len += len;
                return EFI_SUCCESS;
            }

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

        case EfiMemoryMappedIO:
            return mem_type::mmio;

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
        print_string("GetMemoryMap did not return memory map.\n");
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

    while (left >= descriptor_size) {
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

    // sort

    while (true) {
        bool changed = false;

        for (unsigned i = 0; i < num_map_entries - 1; i++) {
            auto& a = map[i];
            auto& b = map[i + 1];

            if (a.addr > b.addr) {
                swap(a, b);
                changed = true;
                i--;
            }
        }

        if (!changed)
            break;
    }

    // FIXME - do we need to resolve any overlaps?

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

#ifdef __aarch64__

static EFI_STATUS map_page(uint64_t va, uint64_t pa) {
    EFI_STATUS status;
    auto pte0 = span((ARM64_HARDWARE_PTE*)ttbr1, EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto level1 = (va & 0xff8000000000) >> 39;

    if (!pte0[level1].valid) {
        EFI_PHYSICAL_ADDRESS new_page;

        status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &new_page);
        if (EFI_ERROR(status)) {
            print_error("AllocatePages", status);
            return status;
        }

        status = add_map_entry(new_page, EFI_PAGE_SIZE, mem_type::paging_table);
        if (EFI_ERROR(status)) {
            print_error("add_map_entry", status);
            return status;
        }

        memset((void*)new_page, 0, EFI_PAGE_SIZE);

        pte0[level1].valid = 1;
        pte0[level1].table_entry = 1;
        pte0[level1].AttrIndex = 0; // FIXME?
        pte0[level1].AP = 0; // no access from userspace
        pte0[level1].SH = 3; // inner shareable
        pte0[level1].AF = 1;
        pte0[level1].pfn = new_page >> EFI_PAGE_SHIFT;
    }

    auto pte1 = span((ARM64_HARDWARE_PTE*)(pte0[level1].pfn << EFI_PAGE_SHIFT),
                     EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto level2 = (va & 0x7fc0000000) >> 30;

    if (!pte1[level2].valid) {
        EFI_PHYSICAL_ADDRESS new_page;

        status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &new_page);
        if (EFI_ERROR(status)) {
            print_error("AllocatePages", status);
            return status;
        }

        status = add_map_entry(new_page, EFI_PAGE_SIZE, mem_type::paging_table);
        if (EFI_ERROR(status)) {
            print_error("add_map_entry", status);
            return status;
        }

        memset((void*)new_page, 0, EFI_PAGE_SIZE);

        pte1[level2].valid = 1;
        pte1[level2].table_entry = 1;
        pte1[level2].AttrIndex = 0; // FIXME?
        pte1[level2].AP = 0; // no access from userspace
        pte1[level2].SH = 3; // inner shareable
        pte1[level2].AF = 1;
        pte1[level2].pfn = new_page >> EFI_PAGE_SHIFT;
    }

    auto pte2 = span((ARM64_HARDWARE_PTE*)(pte1[level2].pfn << EFI_PAGE_SHIFT),
                     EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto level3 = (va & 0x3fe00000) >> 21;

    if (!pte2[level3].valid) {
        EFI_PHYSICAL_ADDRESS new_page;

        status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &new_page);
        if (EFI_ERROR(status)) {
            print_error("AllocatePages", status);
            return status;
        }

        status = add_map_entry(new_page, EFI_PAGE_SIZE, mem_type::paging_table);
        if (EFI_ERROR(status)) {
            print_error("add_map_entry", status);
            return status;
        }

        memset((void*)new_page, 0, EFI_PAGE_SIZE);

        pte2[level3].valid = 1;
        pte2[level3].table_entry = 1;
        pte2[level3].AttrIndex = 0; // FIXME?
        pte2[level3].AP = 0; // no access from userspace
        pte2[level3].SH = 3; // inner shareable
        pte2[level3].AF = 1;
        pte2[level3].pfn = new_page >> EFI_PAGE_SHIFT;
    }

    auto pte3 = span((ARM64_HARDWARE_PTE*)(pte2[level3].pfn << EFI_PAGE_SHIFT),
                     EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto level4 = (va & 0x1ff000) >> 12;

    pte3[level4].valid = 1;
    pte3[level4].table_entry = 1; // sic
    pte3[level4].AttrIndex = 0; // FIXME?
    pte3[level4].AP = 0; // no access from userspace
    pte3[level4].SH = 3; // inner shareable
    pte3[level4].AF = 1;
    pte3[level4].pfn = pa >> EFI_PAGE_SHIFT;

    return EFI_SUCCESS;
}

#else

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

        status = add_map_entry(new_page, EFI_PAGE_SIZE, mem_type::paging_table);
        if (EFI_ERROR(status)) {
            print_error("add_map_entry", status);
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

        status = add_map_entry(new_page, EFI_PAGE_SIZE, mem_type::paging_table);
        if (EFI_ERROR(status)) {
            print_error("add_map_entry", status);
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

        status = add_map_entry(new_page, EFI_PAGE_SIZE, mem_type::paging_table);
        if (EFI_ERROR(status)) {
            print_error("add_map_entry", status);
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
        print("Mapping for {:x} already set.\n", va);
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

#endif

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

#ifdef __aarch64__

static EFI_STATUS setup_lower_half_mappings() {
    EFI_STATUS status;
    EFI_PHYSICAL_ADDRESS addr;
    uint64_t level1;

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &addr);
    if (EFI_ERROR(status)) {
        print_error("AllocatePages", status);
        return status;
    }

    status = add_map_entry(addr, EFI_PAGE_SIZE, mem_type::paging_table);
    if (EFI_ERROR(status)) {
        print_error("add_map_entry", status);
        return status;
    }

    ttbr0 = (ARM64_HARDWARE_PTE*)addr;

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &addr);
    if (EFI_ERROR(status)) {
        print_error("AllocatePages", status);
        return status;
    }

    status = add_map_entry(addr, EFI_PAGE_SIZE, mem_type::paging_table);
    if (EFI_ERROR(status)) {
        print_error("add_map_entry", status);
        return status;
    }

    level1 = addr;

    auto pte0 = span(ttbr0, EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto pte1 = span((ARM64_HARDWARE_PTE*)level1, EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

    memset(pte0.data(), 0, pte0.size_bytes());
    memset(pte1.data(), 0, pte1.size_bytes());

    // FIXME - check ID_AA64MMFR0_EL1 that 4KB page size supported

    pte0[0].valid = 1;
    pte0[0].table_entry = 1;
    pte0[0].AttrIndex = 0;
    pte0[0].AP = 0; // no access from userspace
    pte0[0].SH = 3; // inner shareable
    pte0[0].AF = 1;
    pte0[0].pfn = level1 >> EFI_PAGE_SHIFT;

    unsigned int num_mappable_mem_entries = num_map_entries;

    while (num_mappable_mem_entries > 0 && (map[num_mappable_mem_entries - 1].type == mem_type::free || map[num_mappable_mem_entries - 1].type == mem_type::reserved)) {
        num_mappable_mem_entries--;
    }

    const auto& last_mappable = map[num_mappable_mem_entries - 1];
    static const uint64_t SZ_1G = 0x40000000;

    uint64_t last_address = last_mappable.addr + last_mappable.len;
    auto num_gigs = last_address / SZ_1G;

    if (last_address % SZ_1G)
        num_gigs++;

    // FIXME - fails if would map more than 512GB
    if (num_gigs > EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE))
        num_gigs = EFI_PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE);

    for (unsigned int i = 0; i < num_gigs; i++) {
        pte1[i].valid = 1;
        pte1[i].table_entry = 0;
        pte1[i].AttrIndex = 0;
        pte1[i].AP = 0; // no access from userspace
        pte1[i].SH = 3; // inner shareable
        pte1[i].AF = 1;
        pte1[i].pfn = ((uint64_t)i * SZ_1G) >> EFI_PAGE_SHIFT;
    }

    return EFI_SUCCESS;
}

static EFI_STATUS setup_paging() {
    EFI_STATUS status;
    EFI_PHYSICAL_ADDRESS addr;
    map_entry* map_copy;
    unsigned int num_map_entries_copy;

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &addr);
    if (EFI_ERROR(status)) {
        print_error("AllocatePages", status);
        return status;
    }

    status = add_map_entry(addr, EFI_PAGE_SIZE, mem_type::paging_table);
    if (EFI_ERROR(status)) {
        print_error("add_map_entry", status);
        return status;
    }

    ttbr1 = (ARM64_HARDWARE_PTE*)addr;

    memset((void*)ttbr1, 0, EFI_PAGE_SIZE);

    num_map_entries_copy = num_map_entries;

    status = bs->AllocatePool(EfiLoaderData, sizeof(map_entry) * num_map_entries_copy, (void**)&map_copy);
    if (EFI_ERROR(status)) {
        print_error("AllocatePool", status);
        return status;
    }

    memcpy(map_copy, map, sizeof(map_entry) * num_map_entries);

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

            default:
                break;
        }
    }

    status = setup_lower_half_mappings();
    if (EFI_ERROR(status)) {
        print_error("setup_lower_half_mappings", status);
        return status;
    }

    return EFI_SUCCESS;
}

#else

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

    status = add_map_entry(addr, EFI_PAGE_SIZE, mem_type::paging_table);
    if (EFI_ERROR(status)) {
        print_error("add_map_entry", status);
        return status;
    }

    pml4t = (HARDWARE_PTE*)addr;

    memset((void*)addr, 0, EFI_PAGE_SIZE);

    num_map_entries_copy = num_map_entries;

    status = bs->AllocatePool(EfiLoaderData, sizeof(map_entry) * num_map_entries_copy, (void**)&map_copy);
    if (EFI_ERROR(status)) {
        print_error("AllocatePool", status);
        return status;
    }

    memcpy(map_copy, map, sizeof(map_entry) * num_map_entries);

    for (unsigned int i = 0; i < num_map_entries_copy; i++) {
        switch (map_copy[i].type) {
            case mem_type::bootloader:
            case mem_type::acpi:
            case mem_type::initrd:
            case mem_type::uefi_code:
            case mem_type::uefi_data:
                status = do_map(map_copy[i].addr, map_copy[i].addr, map_copy[i].len); // identity map

                if (EFI_ERROR(status))
                    return status;
                break;

            case mem_type::image:
                status = do_map(KERNEL_ADDR, map_copy[i].addr, map_copy[i].len);

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

#endif

static EFI_STATUS find_acpi_tables(uint64_t& rsdp_pa) {
    EFI_GUID acpi2_guid = ACPI_20_TABLE_GUID;
    RSDP_DESCRIPTOR* rsdp = nullptr;

    for (unsigned int i = 0; i < systable->NumberOfTableEntries; i++) {
        if (!memcmp(&systable->ConfigurationTable[i].VendorGuid, &acpi2_guid, sizeof(EFI_GUID))) {
            rsdp_pa = (uintptr_t)systable->ConfigurationTable[i].VendorTable;
            rsdp = (RSDP_DESCRIPTOR*)systable->ConfigurationTable[i].VendorTable;
            break;
        }
    }

    if (!rsdp) {
        print_string("ACPI 2.0 tables not found\n");
        return EFI_NOT_FOUND;
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

static EFI_STATUS find_initrd_devpath() {
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

static EFI_STATUS open_device_path(char16_t* path, EFI_FILE_HANDLE& file) {
    EFI_GUID path_from_text_guid = EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL* path_from_text;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    EFI_HANDLE h;
    EFI_FILE_HANDLE root;
    EFI_STATUS Status;

    Status = bs->LocateProtocol(&path_from_text_guid, nullptr,
                                (void**)&path_from_text);
    if (EFI_ERROR(Status)) {
        print_error("LocateProtocol", Status);
        return Status;
    }

    auto devpath = path_from_text->ConvertTextToDevicePath((const CHAR16*)path);
    if (!devpath) {
        print("ConvertTextToDevicePath failed\n");
        return EFI_LOAD_ERROR;
    }

    Status = bs->LocateDevicePath(&fs_guid, &devpath, &h);
    if (EFI_ERROR(Status)) {
        bs->FreePool(devpath);
        return EFI_NOT_FOUND;
    }

    Status = bs->HandleProtocol(h, &fs_guid, (void**)&fs);
    if (EFI_ERROR(Status)) {
        bs->FreePool(devpath);
        return EFI_NOT_FOUND;
    }

    if (devpath->Type != MEDIA_DEVICE_PATH || devpath->SubType != MEDIA_FILEPATH_DP) {
        print("unexpected device path node type {:x}, {:x}\n",
              devpath->Type, devpath->SubType);
        bs->FreePool(devpath);
        return EFI_LOAD_ERROR;
    }

    auto fp = (FILEPATH_DEVICE_PATH*)devpath;

    Status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(Status)) {
        print_error("OpenVolume", Status);
        bs->FreePool(devpath);
        return Status;
    }

    Status = root->Open(root, &file, fp->PathName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        print_error("Open", Status);
        bs->FreePool(devpath);
        root->Close(root);
        return Status;
    }

    bs->FreePool(devpath);
    root->Close(root);

    return EFI_SUCCESS;
}

static EFI_STATUS open_volume(char16_t* path, EFI_LOADED_IMAGE_PROTOCOL* image,
                              EFI_FILE_HANDLE& file) {
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    EFI_FILE_HANDLE root;
    EFI_STATUS Status;

    Status = bs->HandleProtocol(image->DeviceHandle, &fs_guid, (void**)&fs);
    if (EFI_ERROR(Status)) {
        print_error("HandleProtocol", Status);
        return Status;
    }

    Status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(Status)) {
        print_error("OpenVolume", Status);
        return Status;
    }

    Status = root->Open(root, &file, (CHAR16*)path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        print_error("Open", Status);
        root->Close(root);
        return Status;
    }

    root->Close(root);

    return EFI_SUCCESS;
}

static EFI_STATUS find_initrd_file(EFI_LOADED_IMAGE_PROTOCOL* image) {
    EFI_GUID info_guid = EFI_FILE_INFO_ID;
    EFI_STATUS Status;
    EFI_FILE_HANDLE file;
    UINTN buf_size;
    char16_t* buf;

    struct {
        EFI_FILE_INFO info;
        char16_t filename[256];
    } file_info;

    static const char16_t prefix[] = u"initrd=";

    auto cmd = wcmdline.find(prefix);
    if (cmd == u16string_view::npos)
        return EFI_NOT_FOUND;

    u16string_view path(wcmdline.data() + cmd + (sizeof(prefix) / sizeof(char16_t)) - 1,
                        wcmdline.size() - cmd - (sizeof(prefix) / sizeof(char16_t)) + 1);

    if (auto sp = path.find(u' '); sp != u16string_view::npos)
        path = u16string_view(path.data(), sp);

    if (path.empty())
        return EFI_NOT_FOUND;

    Status = bs->AllocatePool(EfiBootServicesData, (path.size() + 1) * sizeof(char16_t),
                              (void**)&buf);
    if (EFI_ERROR(Status))
        return Status;

    memcpy(buf, path.data(), path.size() * sizeof(char16_t));
    buf[path.size()] = 0;

    Status = open_device_path(buf, file);

    if (Status == EFI_UNSUPPORTED || Status == EFI_NOT_FOUND) {
        Status = open_volume(buf, image, file);
        bs->FreePool(buf);

        if (EFI_ERROR(Status)) {
            while (true) { }
            return Status;
        }
    } else if (EFI_ERROR(Status)) {
        bs->FreePool(buf);
        return Status;
    }

    bs->FreePool(buf);

    buf_size = sizeof(file_info);
    Status = file->GetInfo(file, &info_guid, &buf_size, &file_info);
    if (EFI_ERROR(Status)) {
        print_error("GetInfo", Status);
        file->Close(file);
        return Status;
    }

    initrd_size = file_info.info.FileSize;

    if (file_info.info.FileSize == 0) {
        file->Close(file);
        return EFI_NOT_FOUND;
    }

    Status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData,
                               bytes_to_pages(initrd_size), &initrd_data);
    if (EFI_ERROR(Status)) {
        file->Close(file);
        return Status;
    }

    auto readbuf = (uint8_t*)initrd_data;

    do {
        // follow Linux in only reading 1MB at a time, to work around dodgy firmware
        UINTN size = max((size_t)(1024 * 1024), (size_t)((uint8_t*)initrd_data + initrd_size - readbuf));

        Status = file->Read(file, &size, readbuf);
        if (EFI_ERROR(Status)) {
            file->Close(file);
            return Status;
        }

        readbuf += size;
    } while (readbuf < (uint8_t*)initrd_data + initrd_size);

    file->Close(file);

    return EFI_SUCCESS;
}

#ifdef __aarch64__

__attribute__((always_inline))
static void inline mair_el1_store(uint64_t v) {
    __asm __volatile (
        "msr\tmair_el1, %0"
        :
        : "r" (v)
    );
}

__attribute__((always_inline))
static uint64_t inline tcr_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs\t%0, tcr_el1"
        : "=r" (ret)
    );

    return ret;
}

__attribute__((always_inline))
static void inline tcr_el1_store(uint64_t v) {
    __asm __volatile (
        "msr\ttcr_el1, %0"
        :
        : "r" (v)
    );
}

#else

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

#endif

#ifdef __aarch64__

__attribute__((noreturn))
static void start_kernel(uint64_t entry_point, loader_block* lb) {
    // FIXME - to EL1 if EL2

    // disable interrupts
    __asm __volatile ("msr DAIFSet, #0xf");

    // FPU already enabled as per spec

    // set MAIR slot 0 to be normal memory
    mair_el1_store(0xff);

    {
        ARM64_TCR_EL1 tcr;

        tcr.value = tcr_el1_load();

        tcr.T0SZ = 16; // 48-bit memory size
        tcr.IRGN0 = 1; // normal memory, inner WB RA WA cacheable
        tcr.ORGN0 = 1; // normal memory, outer WB RA WA cacheable
        tcr.SH0 = 3; // inner shareable
        tcr.TG0 = 0; // 4KB granule size
        tcr.T1SZ = 16; // 48-bit memory size
        tcr.EPD1 = 0; // enable upper-half page-walking
        tcr.IRGN1 = 1; // normal memory, inner WB RA WA cacheable
        tcr.ORGN1 = 1; // normal memory, outer WB RA WA cacheable
        tcr.SH1 = 3; // inner shareable
        tcr.TG1 = 2; // 4KB granule size
        tcr.HA = 0; // hardware access flag disabled

        tcr_el1_store(tcr.value);
    }

    {
        ARM64_SCTLR_EL1 sctlr;

        __asm __volatile (
            "mrs %x[res], sctlr_el1"
            : [res] "=r" (sctlr.value)
        );

        sctlr.M = 1; // enable paging
        sctlr.A = 0; // allow unaligned
        sctlr.C = 1; // enable caching
        sctlr.SA = 1; // stack alignment in kernel mode
        sctlr.SA0 = 1; // stack alignment in user mode
        sctlr.I = 1; // enable instruction caching

        __asm __volatile (
            "msr sctlr_el1, %x[res]"
            :
            : [res] "r" (sctlr.value)
        );
    }

    void (*startup)(loader_block* lb);

    startup = (decltype(startup))entry_point;

    __asm __volatile (
        "msr ttbr0_el1, %0\n\t"
        "msr ttbr1_el1, %1\n\t"
        "tlbi vmalle1\n\t"
        "dsb ish\n\t"
        "isb\n\t"
        "mov x0, %2\n\t"
        "br %3"
        :
        : "r" (ttbr0), "r" (ttbr1), "r" (lb), "r" (startup)
        : "x0"
    );

    __builtin_unreachable();
}

#else

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
        "and rsp, ~0xf\n"
        "call %1\n"
        :
        : "c" (lb),
          "r" (entry_point)
    );

    __builtin_unreachable();
}

#endif

static EFI_STATUS finish_setting_up(EFI_HANDLE image_handle, uint64_t rsdp_pa) {
    EFI_STATUS status;
    UINTN size, key, descsize;
    UINT32 version;
    EFI_MEMORY_DESCRIPTOR* mapdesc;

    lb.map_ptr = (uintptr_t)map;
    lb.num_map_entries = num_map_entries;
    lb.rsdp_pa = rsdp_pa;
#ifndef __aarch64__
    lb.text_mode = false;
#endif
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
    lb.debug_line_ptr = debug_line_ptr;
    lb.debug_line_size = debug_line_size;
    lb.debug_line_str_ptr = debug_line_str_ptr;
    lb.debug_line_str_size = debug_line_str_size;
    lb.GetTime = (decltype(lb.GetTime))systable->RuntimeServices->GetTime;
    lb.SetVirtualAddressMap = (decltype(lb.SetVirtualAddressMap))systable->RuntimeServices->SetVirtualAddressMap;

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

    return EFI_SUCCESS;
}

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

static bool init_gop(EFI_HANDLE h, EFI_HANDLE image_handle, unsigned int preferred_width,
                     unsigned int preferred_height, bool allow_no_edid) {
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GUID edid_guid = EFI_EDID_ACTIVE_PROTOCOL_GUID;
    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;
    EFI_EDID_ACTIVE_PROTOCOL* edp = nullptr;
    unsigned int mode = 0;
    unsigned int pixels = 0;

    status = bs->OpenProtocol(h, &guid, (void**)&gop, image_handle,
                              nullptr, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        print_error("OpenProtocol", status);
        return false;
    }

    status = bs->OpenProtocol(h, &edid_guid, (void**)&edp, image_handle,
                              nullptr, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (!EFI_ERROR(status)) {
        if (edp && edp->SizeOfEdid >= 128) {
            memcpy(edid, edp->Edid, sizeof(edid));
            have_edid = true;
        }

        bs->CloseProtocol(h, &edid_guid, image_handle, nullptr);

        if (!have_edid && !allow_no_edid)
            return false;
    } else if (!allow_no_edid)
        return false;

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
        bs->CloseProtocol(h, &guid, image_handle, nullptr);
        return false;
    }

    lb.framebuffer.phys_address = (uintptr_t)gop->Mode->FrameBufferBase;
    lb.framebuffer.width = gop->Mode->Info->HorizontalResolution;
    lb.framebuffer.height = gop->Mode->Info->VerticalResolution;
    lb.framebuffer.pixels_per_scan_line = gop->Mode->Info->PixelsPerScanLine;

    bs->CloseProtocol(h, &guid, image_handle, nullptr);

    return true;
}

static EFI_STATUS init_framebuffer(EFI_HANDLE image_handle) {
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_HANDLE* handles = NULL;
    UINTN count;
    EFI_STATUS status;
    unsigned int preferred_width = 0, preferred_height = 0;

    parse_video_option(preferred_width, preferred_height);

    status = bs->LocateHandleBuffer(ByProtocol, &guid, NULL, &count, &handles);
    if (EFI_ERROR(status))
        return status;

    if (count == 0) {
        status = EFI_NOT_FOUND;
        goto end;
    }

    // insist on handles that also have an EDID

    for (unsigned int i = 0; i < count; i++) {
        if (!init_gop(handles[i], image_handle, preferred_width,
                      preferred_height, false)) {
            continue;
        }

        status = EFI_SUCCESS;
        goto end;
    }

    // if no luck, allow the other handles too

    for (unsigned int i = 0; i < count; i++) {
        if (!init_gop(handles[i], image_handle, preferred_width,
                      preferred_height, true)) {
            continue;
        }

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
    uint64_t entry_point, img_pa, rsdp_pa;
    size_t img_size;
#ifndef __aarch64__
    uint64_t cr3;
#endif

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

    Status = find_acpi_tables(rsdp_pa);
    if (EFI_ERROR(Status)) {
        print_error("find_acpi_tables", Status);
        return Status;
    }

    Status = find_initrd_devpath();
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
        print_error("find_initrd_devpath", Status);
        return Status;
    }

    if (Status == EFI_NOT_FOUND) {
        Status = find_initrd_file(image);
        if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
            print_error("find_initrd_file", Status);
            return Status;
        }
    }

    Status = init_framebuffer(image_handle);
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
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

    if (initrd_size != 0) {
        Status = add_map_entry(initrd_data, bytes_to_pages(initrd_size) << EFI_PAGE_SHIFT,
                               mem_type::initrd);
        if (EFI_ERROR(Status)) {
            print_error("add_map_entry", Status);
            return Status;
        }
    }

    if (lb.framebuffer.phys_address != 0) {
        Status = add_map_entry((uintptr_t)lb.framebuffer.phys_address,
                               bytes_to_pages(lb.framebuffer.pixels_per_scan_line * lb.framebuffer.height * 4) << EFI_PAGE_SHIFT,
                               mem_type::framebuffer);
        if (EFI_ERROR(Status)) {
            print_error("add_map_entry", Status);
            return Status;
        }
    }

#ifdef __aarch64__
    Status = setup_paging();
    if (EFI_ERROR(Status)) {
        print_error("setup_paging", Status);
        return Status;
    }
#else
    Status = setup_paging(cr3);
    if (EFI_ERROR(Status)) {
        print_error("setup_paging", Status);
        return Status;
    }
#endif

    Status = finish_setting_up(image_handle, rsdp_pa);
    if (EFI_ERROR(Status)) {
        print_error("finish_setting_up", Status);
        return Status;
    }

#ifdef __aarch64__
    start_kernel(entry_point, &lb);
#else
    start_kernel(entry_point, cr3, &lb);
#endif

    // doesn't return unless unsuccessful

    return EFI_LOAD_ERROR;
}
