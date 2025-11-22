module;

#include <stdint.h>

export module guid;

import crt;

export struct guid {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];

    bool operator==(const guid& rhs) const noexcept {
        return !memcmp(this, &rhs, sizeof(guid));
    }
};

static_assert(sizeof(guid) == 16);
