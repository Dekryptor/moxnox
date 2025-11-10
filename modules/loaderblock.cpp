module;

#include <stdint.h>
#include <string_view>
#include <span>

export module loaderblock;

import format;

using namespace std;

using UINTN = uint64_t;
using EFI_PHYSICAL_ADDRESS = uint64_t;
using EFI_VIRTUAL_ADDRESS = uint64_t;

export {

using EFI_STATUS = UINTN;

struct _EFI_GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};

struct EFI_TIME {
    uint16_t Year;
    uint8_t Month;
    uint8_t Day;
    uint8_t Hour;
    uint8_t Minute;
    uint8_t Second;
    uint8_t Pad1;
    uint32_t Nanosecond;
    int16_t TimeZone;
    uint8_t Daylight;
    uint8_t Pad2;
};

struct EFI_TIME_CAPABILITIES {
    uint32_t Resolution;
    uint32_t Accuracy;
    bool SetsToZero;
};

const uint32_t EFI_MEMORY_DESCRIPTOR_VERSION = 1;

enum class _EFI_MEMORY_TYPE : uint32_t {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiUnacceptedMemoryType,
    EfiMaxMemoryType
};

struct _EFI_MEMORY_DESCRIPTOR {
    _EFI_MEMORY_TYPE Type;
    uint32_t Pad;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    uint64_t NumberOfPages;
    uint64_t Attribute;
};

using EFI_GET_TIME = EFI_STATUS (*)(EFI_TIME* Time, EFI_TIME_CAPABILITIES* Capabilities);

using EFI_SET_VIRTUAL_ADDRESS_MAP = EFI_STATUS (*)(UINTN MemoryMapSize,
                                                   UINTN DescriptorSize,
                                                   uint32_t DescriptorVersion,
                                                   _EFI_MEMORY_DESCRIPTOR* VirtualMap);

enum class EFI_RESET_TYPE {
    Cold,
    Warm,
    Shutdown
};

// gnu-efi has the return type as EFI_STATUS, but the spec says void
using EFI_RESET_SYSTEM = EFI_STATUS (*) (EFI_RESET_TYPE ResetType,
                                         EFI_STATUS ResetStatus,
                                         UINTN DataSize, char16_t* ResetData);

const _EFI_GUID EFI_GLOBAL_VARIABLE = { 0x8be4df61, 0x93ca, 0x11d2, {0xaa, 0x0d, 0x00, 0xe0, 0x98, 0x03, 0x2b, 0x8c} };

const uint32_t EFI_VARIABLE_NON_VOLATILE = 0x00000001;
const uint32_t EFI_VARIABLE_BOOTSERVICE_ACCESS = 0x00000002;
const uint32_t EFI_VARIABLE_RUNTIME_ACCESS = 0x00000004;
const uint32_t EFI_VARIABLE_HARDWARE_ERROR_RECORD = 0x00000008;
const uint32_t EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS = 0x00000010;
const uint32_t EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS = 0x00000020;
const uint32_t EFI_VARIABLE_APPEND_WRITE = 0x00000040;
const uint32_t EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS = 0x00000080;

using EFI_SET_VARIABLE = EFI_STATUS (*) (const char16_t* VariableName,
                                         const _EFI_GUID* VendorGuid,
                                         uint32_t Attributes, UINTN DataSize,
                                         void* Data);

enum class mem_type {
    free,
    reserved,
    bootloader,
    image,
    paging_table,
    acpi,
    uefi_code,
    uefi_data,
    framebuffer,
    initrd,
    mmio
};

template<>
consteval bool format_spec_valid<mem_type>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum mem_type t) {
    switch (t) {
        using enum mem_type;

        case free:
            return do_format_arg(sp, "", "free");
        case reserved:
            return do_format_arg(sp, "", "reserved");
        case bootloader:
            return do_format_arg(sp, "", "bootloader");
        case image:
            return do_format_arg(sp, "", "image");
        case paging_table:
            return do_format_arg(sp, "", "paging_table");
        case acpi:
            return do_format_arg(sp, "", "acpi");
        case uefi_code:
            return do_format_arg(sp, "", "uefi_code");
        case uefi_data:
            return do_format_arg(sp, "", "uefi_data");
        case framebuffer:
            return do_format_arg(sp, "", "framebuffer");
        case initrd:
            return do_format_arg(sp, "", "initrd");
        case mmio:
            return do_format_arg(sp, "", "mmio");
        default:
            return do_format_arg(sp, "", (unsigned int)t);
    }
}

const uint64_t EFI_MEMORY_UC = 0x0000000000000001;
const uint64_t EFI_MEMORY_WC = 0x0000000000000002;
const uint64_t EFI_MEMORY_WT = 0x0000000000000004;
const uint64_t EFI_MEMORY_WB = 0x0000000000000008;
const uint64_t EFI_MEMORY_UCE = 0x0000000000000010;
const uint64_t EFI_MEMORY_WP = 0x0000000000001000;
const uint64_t EFI_MEMORY_RP = 0x0000000000002000;
const uint64_t EFI_MEMORY_XP = 0x0000000000004000;
const uint64_t EFI_MEMORY_NV = 0x0000000000008000;
const uint64_t EFI_MEMORY_MORE_RELIABLE = 0x0000000000010000;
const uint64_t EFI_MEMORY_RO = 0x0000000000020000;
const uint64_t EFI_MEMORY_SP = 0x0000000000040000;
const uint64_t EFI_MEMORY_CPU_CRYPTO = 0x0000000000080000;
const uint64_t EFI_MEMORY_RUNTIME = 0x8000000000000000;

struct map_entry {
    uint64_t addr;
    uint64_t len;
    enum mem_type type;
    uint64_t efi_attributes;
} __attribute__((packed));

struct loader_block_framebuffer {
    uint64_t phys_address;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scan_line;
} __attribute__((packed));

#ifndef __aarch64__
struct loader_block_text_console {
    uint16_t cols;
    uint16_t rows;
    uint64_t phys_address;
} __attribute__((packed));
#endif

struct loader_block {
    uint64_t map_ptr;
    uint32_t num_map_entries;
    uint64_t rsdp_pa;
#ifndef __aarch64__
    bool text_mode;
#endif
    union {
        loader_block_framebuffer framebuffer;
#ifndef __aarch64__
        loader_block_text_console text_console;
#endif
    };
    uint64_t cmdline;
    uint64_t initrd_data;
    uint64_t initrd_size;
    uint64_t edid;
    uint64_t debug_info_ptr;
    uint64_t debug_info_size;
    uint64_t debug_abbrev_ptr;
    uint64_t debug_abbrev_size;
    uint64_t debug_rnglists_ptr;
    uint64_t debug_rnglists_size;
    uint64_t debug_str_ptr;
    uint64_t debug_str_size;
    uint64_t debug_addr_ptr;
    uint64_t debug_addr_size;
    uint64_t debug_str_offsets_ptr;
    uint64_t debug_str_offsets_size;
    uint64_t debug_line_ptr;
    uint64_t debug_line_size;
    uint64_t debug_line_str_ptr;
    uint64_t debug_line_str_size;
    EFI_GET_TIME GetTime;
    EFI_SET_VIRTUAL_ADDRESS_MAP SetVirtualAddressMap;
    EFI_RESET_SYSTEM ResetSystem;
    EFI_SET_VARIABLE SetVariable;
} __attribute__((packed));

const uint64_t SELFMAP = 0xfffff68000000000;

}
