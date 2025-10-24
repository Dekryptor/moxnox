OUTPUT_FORMAT("binary")
OUTPUT_ARCH("aarch64")
TARGET("pe-aarch64-little")
SECTIONS
{
    .boot 0 : AT(0) {
        image_base = .;
        *(.boot)
    }

    . = ALIGN(4096);

    .text : AT(text) {
        text = .;
        *boot.S.obj(.text)
        *boot.cpp.obj(.text)
        *mem.S.obj(.text)
        *pe.cpp.obj(.text)
        *loaderblock.cpp.obj(.text)
    }

    . = ALIGN(4096);

    .stack : AT(stack) {
        stack = .;
        *(.stack)
    }

    . = ALIGN(4096);

    .data : AT(data) {
        data = .;
        *boot.S.obj(.rdata.*)
        *boot.cpp.obj(.rdata.*)
        *mem.S.obj(.rdata.*)
        *payload.s.obj(.rdata.*)
        *boot.S.obj(.data.*)
        *boot.cpp.obj(.data.*)
        *mem.S.obj(.data.*)
        *payload.s.obj(.data.*)
    }

    . = ALIGN(4096);

    .payload : AT(payload) {
        efi_payload = ABSOLUTE(.);
        payload = .;
        *payload.s.obj(.payload)
        . = ALIGN(4096);
        efi_payload_size = ABSOLUTE(.) - efi_payload;
    }

    . = ALIGN(4096);

    .efi_text : AT(efi_text) {
        efi_text = ABSOLUTE(.);
        *efi.cpp.obj(.text)
        . = ALIGN(4096);
        efi_text_size = ABSOLUTE(.) - efi_text;
    }

    . = ALIGN(4096);

    .efi_data : AT(efi_data) {
        efi_data = ABSOLUTE(.);
        *efi.cpp.obj(.data)
        . = ALIGN(4096);
        efi_data_size = ABSOLUTE(.) - efi_data;
    }

    . = ALIGN(4096);

    .efi_rdata : {
        efi_rdata = ABSOLUTE(.);
        *efi.cpp.obj(.rdata)
        . = ALIGN(4096);
        efi_rdata_size = ABSOLUTE(.) - efi_rdata;
    }

    efi_rdata_vma = 0x1000 + efi_text_size + efi_data_size;
    efi_payload_vma = 0x1000 + efi_text_size + efi_data_size + efi_rdata_size;

    size_of_image = efi_text_size + efi_data_size + efi_rdata_size + efi_payload_size + 0x1000;
    efi_entry_point = efi_main - efi_text + 0x1000;
    size_of_initialized_data = efi_data_size + efi_rdata_size + efi_payload_size;

    image_end = .;
    image_size_val = image_end - image_base;
}
