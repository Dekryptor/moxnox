#pragma once

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define CR0_MP      (1 << 1)
#define CR0_WP      (1 << 16)
#define CR0_PG      (1 << 31)
