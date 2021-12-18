.intel_syntax noprefix
.code16

.global ap_trampoline
.global ap_trampoline_end

ap_trampoline:
    xor eax, eax
    jmp eax

ap_trampoline_end:
