#pragma once

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define CR0_MP      (1 << 1)
#define CR0_WP      (1 << 16)
#define CR0_PG      (1 << 31)

#define CR4_PAE         (1 << 5)
#define CR4_OSFXSR      (1 << 9)
#define CR4_OSXMMEXCPT  (1 << 10)

#define EFER_MSR    0xc0000080

#define EFER_SCE    (1 << 0)
#define EFER_LME    (1 << 8)
#define EFER_NX     (1 << 11)
