module;

#include <stdint.h>
#include <string_view>
#include <span>

export module loaderblock;

import format;

#ifndef _X86_
import gnu_efi;
#endif

using namespace std;

export {

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

#if defined(__x86_64__) || defined(_X86_)
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
#if defined(__x86_64__) || defined(_X86_)
    bool text_mode;
#endif
    union {
        loader_block_framebuffer framebuffer;
#if defined(__x86_64__) || defined(_X86_)
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
    char version[32];
#ifdef _X86_ // avoid bringing in gnu-efi
    uint64_t GetTime;
    uint64_t SetVirtualAddressMap;
    uint64_t ResetSystem;
    uint64_t SetVariable;
#else
    EFI_GET_TIME GetTime;
    EFI_SET_VIRTUAL_ADDRESS_MAP SetVirtualAddressMap;
    EFI_RESET_SYSTEM ResetSystem;
    EFI_SET_VARIABLE SetVariable;
#endif
} __attribute__((packed));

const uint64_t SELFMAP = 0xfffff68000000000;

}
