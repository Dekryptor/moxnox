.intel_syntax noprefix
.code64

.global efi_startup2
efi_startup2:
# FIXME
jmp efi_startup2

.data
.asciz "hello world!\r\n"
