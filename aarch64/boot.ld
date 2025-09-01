OUTPUT_FORMAT("binary")
OUTPUT_ARCH("aarch64")
TARGET("pe-aarch64-little")
SECTIONS
{
    .boot 0 : AT(0) {
        image_base = .;
        *(.boot)
        . = ALIGN(4096);
    }
    .text : AT(text) {
        text = .;
        *(.text)
        . = ALIGN(4096);
    }
    .stack : AT(stack) {
        stack = .;
        *(.stack)
        . = ALIGN(4096);
    }
    .rdata : AT(rdata) {
        rdata = .;
        *payload.o(.rdata)
        . = ALIGN(4096);
    }
    .payload : AT(payload) {
        payload = .;
        *payload.o(.payload)
        . = ALIGN(4096);
    }
}
