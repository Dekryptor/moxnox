OUTPUT_FORMAT("binary")
SECTIONS
{
    .boot 0 : AT(0) {
        boot.o(.boot)
        . = ALIGN(4096);
    }
    .data : AT(data) {
        data = .;
        *(.data)
    }

    . = ALIGN(16);
    .bss : {
        __bss_start = .;
        *(.bss)
        __bss_end  = .;
    }
    . = ALIGN(16);
    _end = .;

    .text : AT(text) {
        text = .;
        *(.text)
        . = ALIGN(4096);
        setup_size = ABSOLUTE(.);
        setup_sects = ABSOLUTE(setup_size / 512);
    }

/*    .entrytext : AT(entry_text) {
        entry_text = .;
        boot.o(.entrytext)
    }*/
    
/*     .boot phys : SUBALIGN(0) { */
/*         boot.o( .boot ) */
/*     } */
/*     .entrytext 100 : { */
/*         boot.o( .entrytext ) */
/*         . = ALIGN(4096); */
/*     } */
}
