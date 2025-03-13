.intel_syntax noprefix
.code16

.section .rm_text
.globl enter_protected_mode
enter_protected_mode:
    cli

    # change GDT pointer to flat address
    mov eax, ds
    shl eax, 4
    or eax, DWORD PTR [gdt_descriptor + 2]
    mov DWORD PTR [gdt_descriptor + 2], eax

    lgdt [gdt_descriptor]

    mov eax, cr0
    or al, 1
    mov cr0, eax

    mov ax, cs
    shl eax, 4
    or eax, OFFSET 1f

    push 0x8
    push eax

    # 32-bit retf
    .byte 0x66
    .byte 0xcb

.code32
1:
    mov ax, 0x10
    mov ebx, ds
    mov ds, ax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [_ptr_e820_entries]
    mov DWORD PTR [_ptr_e820_entries], eax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [_ptr_e820_count]
    mov DWORD PTR [_ptr_e820_count], eax

    mov eax, ebx
    shl eax, 4
    mov ecx, DWORD PTR [_ptr_cmdline]
    or eax, ecx
    mov ecx, DWORD PTR [eax]
    mov eax, ebx
    shl eax, 4
    or eax, ecx
    mov eax, DWORD PTR [eax]
    mov DWORD PTR [_ptr_cmdline], eax

    mov eax, ebx
    shl eax, 4
    mov ecx, DWORD PTR [_ptr_initrd_data]
    or eax, ecx
    mov ecx, DWORD PTR [eax]
    mov eax, ebx
    shl eax, 4
    or eax, ecx
    mov eax, DWORD PTR [eax]
    mov DWORD PTR [_ptr_initrd_data], eax

    mov eax, ebx
    shl eax, 4
    mov ecx, DWORD PTR [_ptr_initrd_size]
    or eax, ecx
    mov ecx, DWORD PTR [eax]
    mov eax, ebx
    shl eax, 4
    or eax, ecx
    mov DWORD PTR [_ptr_initrd_size], eax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [_pm_vbe_selected_mode]
    mov ax, WORD PTR [eax]
    and eax, 0xffff
    mov DWORD PTR [_pm_vbe_selected_mode], eax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [_ptr_vbe_modes]
    mov DWORD PTR [_ptr_vbe_modes], eax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [_ptr_vbe_edid_valid]
    mov DWORD PTR [_ptr_vbe_edid_valid], eax

    mov eax, ebx
    shl eax, 4
    or eax, DWORD PTR [_ptr_vbe_edid]
    mov DWORD PTR [_ptr_vbe_edid], eax

    mov ax, 0x10
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    and esp, ~3

    mov eax, OFFSET _protected_mode_main
    jmp eax

.code32
.section .text
.globl _have_long_mode
_have_long_mode:
    push ebx

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb 1f

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz 1f

    mov eax, 1
    pop ebx
    ret

1:
    xor eax, eax
    pop ebx
    ret

.code64
.section .text
.globl long_mode_start
long_mode_start:
    # mov rsp, 0xfffff90000007fe0 # KERNEL_STACK + (KERNEL_STACK_PAGES << PAGE_SHIFT) - 0x20 (0x20 for MS ABI)
    shl rdx, 32
    mov eax, eax # clear top half
    or rdx, rax
    mov rsp, rdx
    xor rbp, rbp

    lea rax, _entry_point[rip]
    mov rax, [rax]
    lea rcx, _lb[rip]
    call rax

.section .data
.globl _ptr_e820_entries
_ptr_e820_entries:
.long e820_entries

.globl _ptr_e820_count
_ptr_e820_count:
.long e820_count

.globl _ptr_cmdline
_ptr_cmdline:
.long cmdline

.globl _ptr_initrd_data
_ptr_initrd_data:
.long initrd_data

.globl _ptr_initrd_size
_ptr_initrd_size:
.long initrd_size

.global _pm_vbe_selected_mode
_pm_vbe_selected_mode:
.long vbe_selected_mode

.global _ptr_vbe_modes
_ptr_vbe_modes:
.long vbe_modes

.global _ptr_vbe_edid_valid
_ptr_vbe_edid_valid:
.long vbe_edid_valid

.global _ptr_vbe_edid
_ptr_vbe_edid:
.long vbe_edid
