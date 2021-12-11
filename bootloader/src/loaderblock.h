#pragma once

#include <stdint.h>

enum map_type {
    mem_free,
    mem_reserved,
    mem_bootloader,
    mem_image,
    mem_paging_table,
    mem_stack
};

typedef struct {
    uint64_t addr;
    uint64_t len;
    enum map_type type;
} map_entry;

// FIXME - add magic to loader block?

typedef struct {
    map_entry* map;
    unsigned int num_map_entries;
} loader_block;
