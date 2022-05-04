#include "mbox.h"

static unsigned int width, height, pitch, isrgb;
static uint32_t* fb;

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
        uintptr_t fb_addr = mbox[28];

        fb_addr &= 0x3fffffff; // Convert GPU address to ARM address
        width = mbox[10];       // Actual physical width
        height = mbox[11];      // Actual physical height
        pitch = mbox[33];       // Number of bytes per line
        isrgb = mbox[24];       // Pixel order

        fb = (uint32_t*)fb_addr;

        fb[0] = 0xffffff;
    }
}

extern "C"
void main() {
    // FIXME - parse dtb

    init_framebuffer();

    while (1) {
    }
}
