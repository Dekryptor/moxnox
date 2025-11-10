#pragma once

union ARM64_ESR {
    struct {
        uint64_t ISS : 25;
        uint64_t IL : 1;
        uint64_t EC : 6;
        uint64_t ISS2 : 24;
    };
    uint64_t value;
} __attribute((packed));
