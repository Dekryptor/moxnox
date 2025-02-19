module;

#include <stdint.h>

export module inout;

export void outb(uint16_t port, uint8_t val) {
    __asm __volatile(
        "mov\tal, %1\n"
        "\tmov\tdx, %0\n"
        "\tout\tdx, al\n"
        :
        : "r" (port), "r" (val)
        : "al", "dx"
    );
}
