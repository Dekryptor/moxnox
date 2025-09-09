#pragma once

#define SELFMAP_PDT             0xfffff6fb40000000
#define SELFMAP_PDPT            0xfffff6fb7da00000
#define SELFMAP_PML4            0xfffff6fb7dbed000

struct HARDWARE_PTE {
    uint64_t p : 1;
    uint64_t w : 1;
    uint64_t u : 1;
    uint64_t pwt : 1;
    uint64_t pcd : 1;
    uint64_t a : 1;
    uint64_t d : 1;
    uint64_t ps : 1;
    uint64_t g : 1;
    uint64_t allow_write : 1; // custom
    uint64_t cow : 1; // custom
    uint64_t reserved : 1; // custom
    uint64_t pfn : 51;
    uint64_t xd : 1;
};

static_assert(sizeof(HARDWARE_PTE) == sizeof(uint64_t));

struct gdt_entry {
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint32_t BaseMid:8;
    uint32_t Type:5;
    uint32_t Dpl:2;
    uint32_t Pres:1;
    uint32_t LimitHi:4;
    uint32_t Sys:1;
    uint32_t Long:1;
    uint32_t Default_Big:1;
    uint32_t Granularity:1;
    uint32_t BaseHi:8;
};

static_assert(sizeof(gdt_entry) == 8);

struct task_state_segment {
    uint32_t reserved1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iopb_offset;
} __attribute__((packed));

static_assert(sizeof(task_state_segment) == 0x68);

static const uint16_t TSS_SEGMENT = 0x18;
