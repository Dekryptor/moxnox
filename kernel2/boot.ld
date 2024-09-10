OUTPUT_FORMAT("binary")
OUTPUT_ARCH("i386")
TARGET("pe-i386")
SECTIONS
{
    .boot 0 : AT(0) {
        *(.boot)
        . = ALIGN(4096);
    }
    .data : AT(data) {
        data = .;
        *(.data)
    }

    . = ALIGN(16);
    .rdata : {
        *(.rdata)
    }

    . = ALIGN(16);
    .bss : {
        __bss_start = .;
        *(.bss)
        __bss_end  = .;
    }

    . = ALIGN(16);
    _end = .;

    .rm_text : AT(rm_text) {
        rm_text = .;
        *(.rm_text)
        . = ALIGN(4096);
        setup_size = ABSOLUTE(.);
        setup_sects = ABSOLUTE(setup_size / 512);
    }

    .text 0x100000 : AT(setup_size) {
        text = 0x100000;
        *(.text)
        . = ALIGN(4096);
        text_end = ABSOLUTE(.);
        syssize = (text_end - ABSOLUTE(text)) / 16;
    }
}
