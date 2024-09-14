OUTPUT_FORMAT("binary")
OUTPUT_ARCH("i386")
TARGET("pe-i386")
SECTIONS
{
    .boot 0 : AT(0) {
        *(.boot)
        . = ALIGN(4096);
    }
    .rm_data : AT(rm_data) {
        rm_data = .;
        *rm-main.o(.data)
    }

    . = ALIGN(16);
    .rm_rdata : {
        *rm-main.o(.rdata)
    }

    . = ALIGN(16);
    .rm_bss : {
        __rm_bss_start = .;
        *rm-main.o(.bss)
        __rm_bss_end = .;
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
        text = .;
        *pm-main.o(.text)
        . = ALIGN(4096);
    }

    .bss : {
        _bss_start = .;
        *pm-main.o(.bss)
        _bss_end = .;
        . = ALIGN(16);
    }

    .rdata : {
        *pm-main.o(.rdata)
        . = ALIGN(16);
    }

    .data : ALIGN(4096) {
        *pm-main.o(.data)
        *payload.o(.data)
        . = ALIGN(4096);
        pm_end = .;
    }

    syssize = (ABSOLUTE(pm_end) - ABSOLUTE(text)) / 16;

    .payload : {
        *payload.o(.payload)
    }
}
