// SPDX-License-Identifier: GPL-2.0-only
module;

#include <stdint.h>
#include <span>
#include <bit>

export module chacha20;

using namespace std;

static void QR(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b;
    d ^= a;
    d = rotl(d, 16);

    c += d;
    b ^= c;
    b = rotl(b, 12);

    a += b;
    d ^= a;
    d = rotl(d, 8);

    c += d;
    b ^= c;
    b = rotl(b, 7);
}

export void chacha_block(span<uint32_t, 16> out, span<const uint32_t, 16> in) {
    uint32_t x[16];

    for (unsigned int i = 0; i < 16; i++) {
        x[i] = in[i];
    }

    for (unsigned int i = 0; i < 20; i += 2) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);

        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }

    for (unsigned int i = 0; i < 16; i++) {
        out[i] = x[i] + in[i];
    }
}
