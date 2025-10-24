module;

#include <stdint.h>

export module loaderblock;

export {

enum class mem_type {
    free,
    reserved,
    bootloader,
    image,
    paging_table,
    stack,
    acpi,
    uefi_code,
    uefi_data,
    framebuffer,
    initrd
};

struct map_entry {
    uint64_t addr;
    uint64_t len;
    enum mem_type type;
} __attribute__((packed));

struct loader_block_framebuffer {
    uint64_t phys_address;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scan_line;
} __attribute__((packed));

struct loader_block_text_console {
    uint16_t cols;
    uint16_t rows;
    uint64_t phys_address;
} __attribute__((packed));

struct loader_block {
    uint64_t map_ptr;
    uint32_t num_map_entries;
    uint64_t rsdp_pa;
    bool text_mode;
    union {
        loader_block_framebuffer framebuffer;
        loader_block_text_console text_console;
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
} __attribute__((packed));

const uint64_t SELFMAP = 0xfffff68000000000;

}
