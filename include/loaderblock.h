#pragma once

#include <stdint.h>

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
    framebuffer
};

struct map_entry {
    uint64_t addr;
    uint64_t len;
    enum mem_type type;
} __attribute__((packed));

struct loader_block_framebuffer {
    void* pa;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scan_line;
};

struct loader_block {
    uint64_t map_ptr;
    uint32_t num_map_entries;
    uint64_t xsdt_pa;
    loader_block_framebuffer framebuffer;
} __attribute__((packed));
