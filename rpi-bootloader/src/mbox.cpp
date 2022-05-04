#include "mbox.h"

volatile uint32_t __attribute__((aligned(16))) mbox[36];

static void mmio_write(uintptr_t reg, uint32_t val) {
    *(volatile uint32_t*)reg = val;
}

static uint32_t mmio_read(uintptr_t reg) {
    return *(volatile uint32_t*)reg;
}

bool mbox_call(enum MBOX_CH ch) {
    uint32_t r = ((uint32_t)((uintptr_t)&mbox) &~ 0xf) | (uint8_t)ch;

    while (mmio_read(MBOX_STATUS) & MBOX_FULL) { }

    mmio_write(MBOX_WRITE, r);

    while (1) {
        while (mmio_read(MBOX_STATUS) & MBOX_EMPTY) { }

        if (mmio_read(MBOX_READ) == r)
            return mbox[1] == MBOX_RESPONSE;
    }
}
