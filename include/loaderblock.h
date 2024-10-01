#pragma once

#include <stdint.h>

enum class map_type {
    mem_free,
    mem_reserved,
    mem_bootloader,
    mem_image,
    mem_paging_table,
    mem_stack,
    mem_acpi,
    mem_uefi_code,
    mem_uefi_data,
    mem_framebuffer
};

struct map_entry {
    uint64_t addr;
    uint64_t len;
    enum map_type type;
} __attribute__((packed));

struct loader_block {
    uint64_t map_ptr;
    uint32_t num_map_entries;
    uint64_t xsdt_pa;

    struct {
        void* pa;
        uint32_t width;
        uint32_t height;
        uint32_t pixels_per_scan_line;
    } framebuffer;
} __attribute__((packed));
