#pragma once

struct ARM64_HARDWARE_PTE {
    uint64_t valid : 1;
    uint64_t table_entry : 1;
    uint64_t AttrIndex : 3;
    uint64_t NS : 1;
    uint64_t AP : 2;
    uint64_t SH : 2;
    uint64_t AF : 1;
    uint64_t nG : 1;
    uint64_t pfn : 39;
    uint64_t DBM : 1;
    uint64_t Contig : 1;
    uint64_t PXN : 1;
    uint64_t UXN : 1;
    uint64_t software : 4;
    uint64_t imp_def : 4;
    uint64_t : 1;
} __attribute((packed));

static_assert(sizeof(ARM64_HARDWARE_PTE) == 8);
