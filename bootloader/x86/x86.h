#pragma once

#include <span>

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

struct HARDWARE_PTE_PAE {
    uint64_t Valid:1;
    uint64_t Write:1;
    uint64_t Owner:1;
    uint64_t WriteThrough:1;
    uint64_t CacheDisable:1;
    uint64_t Accessed:1;
    uint64_t Dirty:1;
    uint64_t LargePage:1;
    uint64_t Global:1;
    uint64_t CopyOnWrite:1;
    uint64_t Prototype:1;
    uint64_t reserved0:1;
    uint64_t PageFrameNumber:28;
    uint64_t reserved1:12;
    uint64_t SoftwareWsIndex:11;
    uint64_t NoExecute:1;
};

struct GDTIDT {
    uint16_t Limit;
    uint32_t Base;
} __attribute__((packed));

using pte_span = std::span<HARDWARE_PTE_PAE, PAGE_SIZE / sizeof(HARDWARE_PTE_PAE)>;
