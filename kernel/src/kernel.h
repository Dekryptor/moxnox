#pragma once

#include <stdint.h>

typedef struct _loader_block loader_block;

// mem.cpp
bool init_memory(const loader_block& lb);
