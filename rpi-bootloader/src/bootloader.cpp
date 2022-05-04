#include <stdint.h>

static volatile uint32_t __attribute__((aligned(16))) mbox[36];

static unsigned int width, height, pitch, isrgb;
static unsigned char* fb;

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

enum {
    MBOX_REQUEST  = 0
};

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

void mmio_write(uintptr_t reg, uint32_t val) { *(volatile uint32_t*)reg = val; }
uint32_t mmio_read(uintptr_t reg) { return *(volatile uint32_t*)reg; }

static bool mbox_call(enum MBOX_CH ch) {
    uint32_t r = ((uint32_t)((uintptr_t)&mbox) &~ 0xF) | (uint8_t)ch;

    while (mmio_read(MBOX_STATUS) & MBOX_FULL);

    mmio_write(MBOX_WRITE, r);

    while (1) {
        while (mmio_read(MBOX_STATUS) & MBOX_EMPTY) { }

        if (r == mmio_read(MBOX_READ))
            return mbox[1] == MBOX_RESPONSE;
    }
}

static void init_framebuffer() {
    // FIXME
    mbox[0] = 35*4; // Length of message in bytes
    mbox[1] = MBOX_REQUEST;

    mbox[2] = (uint32_t)MBOX_TAG::SETPHYWH; // Tag identifier
    mbox[3] = 8; // Value size in bytes
    mbox[4] = 0;
    mbox[5] = 1920; // Value(width)
    mbox[6] = 1080; // Value(height)

    mbox[7] = (uint32_t)MBOX_TAG::SETVIRTWH;
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = 1920;
    mbox[11] = 1080;

    mbox[12] = (uint32_t)MBOX_TAG::SETVIRTOFF;
    mbox[13] = 8;
    mbox[14] = 8;
    mbox[15] = 0; // Value(x)
    mbox[16] = 0; // Value(y)

    mbox[17] = (uint32_t)MBOX_TAG::SETDEPTH;
    mbox[18] = 4;
    mbox[19] = 4;
    mbox[20] = 32; // Bits per pixel

    mbox[21] = (uint32_t)MBOX_TAG::SETPXLORDR;
    mbox[22] = 4;
    mbox[23] = 4;
    mbox[24] = 1; // RGB

    mbox[25] = (uint32_t)MBOX_TAG::GETFB;
    mbox[26] = 8;
    mbox[27] = 8;
    mbox[28] = 4096; // FrameBufferInfo.pointer
    mbox[29] = 0;    // FrameBufferInfo.size

    mbox[30] = (uint32_t)MBOX_TAG::GETPITCH;
    mbox[31] = 4;
    mbox[32] = 4;
    mbox[33] = 0; // Bytes per line

    mbox[34] = (uint32_t)MBOX_TAG::LAST;

    // Check call is successful and we have a pointer with depth 32
    if (mbox_call(MBOX_CH::PROP) && mbox[20] == 32 && mbox[28] != 0) {
        mbox[28] &= 0x3FFFFFFF; // Convert GPU address to ARM address
        width = mbox[10];       // Actual physical width
        height = mbox[11];      // Actual physical height
        pitch = mbox[33];       // Number of bytes per line
        isrgb = mbox[24];       // Pixel order
        fb = (unsigned char *)((uintptr_t)mbox[28]);

        fb[0] = 0xff;
        fb[1] = 0xff;
        fb[2] = 0xff;
    }
}

extern "C"
void main() {
    // FIXME - parse dtb

    init_framebuffer();

    while (1) {
    }
}
