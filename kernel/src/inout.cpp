module;

#include <stdint.h>

export module inout;

export uint8_t inb(uint16_t port) {
    uint8_t ret;

    __asm __volatile(
        "\tin %b0, %w1\n"
        : "=a" (ret)
        : "Nd" (port)
        :
    );

    return ret;
}

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

export void outw(uint16_t port, uint16_t val) {
    __asm __volatile(
        "mov\tax, %1\n"
        "\tmov\tdx, %0\n"
        "\tout\tdx, ax\n"
        :
        : "r" (port), "r" (val)
        : "ax", "dx"
    );
}
