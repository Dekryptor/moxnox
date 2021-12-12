#pragma once

#include <stdint.h>

typedef struct _loader_block loader_block;

// mem.cpp
bool init_memory(const loader_block& lb);
bool map_page(uint64_t pa, uint64_t va);

// console.cpp
bool init_console();
