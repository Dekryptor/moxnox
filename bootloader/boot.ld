OUTPUT_FORMAT("binary")
OUTPUT_ARCH("i386")
TARGET("pe-i386")
/* OUTPUT_ARCH("i386:x86-64") */
/* TARGET("pe-x86-64") */
SECTIONS
{
    .boot 0 : AT(0) {
        *(.boot)
        . = ALIGN(4096);
    }

    .rm_data : AT(rm_data) {
        rm_data = .;
        *rm-main.cpp.obj(.data)
        *boot.S.obj(.data)
    }

    .vbe : AT(vbe) {
        vbe = .;
        *boot.S.obj(.vbe)
    }

    . = ALIGN(16);
    .rm_rdata : {
        *rm-main.cpp.obj(.rdata)
        *boot.S.obj(.rdata)
    }

    . = ALIGN(16);
    .rm_bss : {
        __rm_bss_start = .;
        *rm-main.cpp.obj(.bss)
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
        *pm-main.cpp.obj(.text)
        . = ALIGN(4096);
    }

    .bss : {
        _bss_start = .;
        *pm-main.cpp.obj(.bss)
        _bss_end = .;
        . = ALIGN(16);
    }

    .rdata : {
        *pm-main.cpp.obj(.rdata)
        *payload.s.obj(.rdata)
        . = ALIGN(16);
    }

    .data : ALIGN(4096) {
        *pm-main.cpp.obj(.data)
        *pm.s.obj(.data)
        . = ALIGN(4096);
        pm_end = .;
    }

    syssize = (ABSOLUTE(pm_end) - ABSOLUTE(text)) / 16;

    .payload : {
        *payload.s.obj(.payload)
        . = ALIGN(4096);
        efi_begin = ABSOLUTE(.) - 0x100000 + setup_size;
    }

    .efi_text : AT(efi_begin) {
        efi_text = ABSOLUTE(.);
        *efi.cpp.obj(.text)
        . = ALIGN(4096);
        efi_text_size = ABSOLUTE(.) - efi_text;
    }

    .efi_data : {
        efi_data_begin = ABSOLUTE(.) - 0x100000 + setup_size;
        efi_data = ABSOLUTE(.);
        *efi.cpp.obj(.data)
        . = ALIGN(4096);
        efi_data_size = ABSOLUTE(.) - efi_data;
    }

    .efi_rdata : {
        efi_rdata_begin = ABSOLUTE(.) - 0x100000 + setup_size;
        efi_rdata = ABSOLUTE(.);
        *efi.cpp.obj(.rdata)
        . = ALIGN(4096);
        efi_rdata_size = ABSOLUTE(.) - efi_rdata;
    }

    efi_rdata_vma = 0x1000 + efi_text_size + efi_data_size;

    size_of_image = efi_text_size + efi_data_size + efi_rdata_size + 0x1000;
    efi_entry_point = efi_main - efi_text;
    size_of_initialized_data = efi_data_size + efi_rdata_size;
}
