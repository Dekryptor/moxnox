OUTPUT_FORMAT("binary")
OUTPUT_ARCH("aarch64")
TARGET("pe-aarch64-little")
SECTIONS
{
    .boot 0 : AT(0) {
        *(.boot)
        . = ALIGN(4096);
    }
    .text : AT(text) {
        text = .;
        *(.text)
    }
}
