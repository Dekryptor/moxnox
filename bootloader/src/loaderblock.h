#pragma once

#include <stdint.h>

#ifdef __cplusplus
enum class map_type
#else
enum map_type
#endif
{
    mem_free,
    mem_reserved,
    mem_bootloader,
    mem_image,
    mem_paging_table,
    mem_stack,
    mem_acpi
};

#pragma pack(push,1)

typedef struct {
    uint64_t addr;
    uint64_t len;
    enum map_type type;
} map_entry;

// FIXME - add magic to loader block?

typedef struct _loader_block {
#ifdef __i386__
    uint64_t map;
#else
    map_entry* map;
#endif
    uint32_t num_map_entries;
    uint64_t rsdt_pa;
} loader_block;

#pragma pack(pop)
