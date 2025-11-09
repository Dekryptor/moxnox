#pragma once

static const uint64_t ARM64_PTE_AP_EL1RW = 0;
static const uint64_t ARM64_PTE_AP_EL0RW = 1;
static const uint64_t ARM64_PTE_AP_EL1RO = 2;
static const uint64_t ARM64_PTE_AP_EL0RO = 3;

union ARM64_ESR {
    struct {
        uint64_t ISS : 25;
        uint64_t IL : 1;
        uint64_t EC : 6;
        uint64_t ISS2 : 24;
    };
    uint64_t value;
} __attribute((packed));
