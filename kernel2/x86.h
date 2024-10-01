#pragma once

#include <span>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define CR0_PG      0x80000000

#define CR4_PAE     0x00000020

#define EFER_MSR    0xc0000080

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

#define NUM_IDT 256

struct GDTIDT {
    uint16_t Limit;
    uint32_t Base;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attributes;
    uint16_t offset_2;
} __attribute__((packed));

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
} __attribute__((packed));

using pte_span = std::span<HARDWARE_PTE_PAE, PAGE_SIZE / sizeof(HARDWARE_PTE_PAE)>;

enum class e820_mem_type : uint32_t {
    free = 1,
    reserved = 2,
    acpi_reclaimable = 3,
    acpi_nvs = 4,
    bad = 5,
};

struct e820_entry {
    uint64_t address;
    uint64_t length;
    enum e820_mem_type type;
} __attribute__((packed));
