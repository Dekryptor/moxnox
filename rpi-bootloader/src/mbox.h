#pragma once

#include <stdint.h>

extern volatile uint32_t mbox[36];

#define PERIPHERAL_BASE 0x3f000000

enum {
    VIDEOCORE_MBOX = (PERIPHERAL_BASE + 0x0000B880),
    MBOX_READ      = (VIDEOCORE_MBOX + 0x0),
    MBOX_POLL      = (VIDEOCORE_MBOX + 0x10),
    MBOX_SENDER    = (VIDEOCORE_MBOX + 0x14),
    MBOX_STATUS    = (VIDEOCORE_MBOX + 0x18),
    MBOX_CONFIG    = (VIDEOCORE_MBOX + 0x1C),
    MBOX_WRITE     = (VIDEOCORE_MBOX + 0x20),
    MBOX_RESPONSE  = 0x80000000,
    MBOX_FULL      = 0x80000000,
    MBOX_EMPTY     = 0x40000000
};

static const uint32_t MBOX_REQUEST = 0;

enum class MBOX_CH : uint8_t {
    POWER = 0,
    FB = 1,
    VUART = 2,
    VCHIQ = 3,
    LEDS = 4,
    BTNS = 5,
    TOUCH = 6,
    COUNT = 7,
    PROP = 8
};

enum class MBOX_TAG : uint32_t {
    SETPOWER = 0x28001,
    SETCLKRATE = 0x38002,
    SETPHYWH = 0x48003,
    SETVIRTWH = 0x48004,
    SETVIRTOFF = 0x48009,
    SETDEPTH = 0x48005,
    SETPXLORDR = 0x48006,
    GETFB = 0x40001,
    GETPITCH = 0x40008,
    LAST = 0
};

bool mbox_call(enum MBOX_CH ch);
