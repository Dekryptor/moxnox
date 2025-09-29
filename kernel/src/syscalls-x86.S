.intel_syntax noprefix

#include "context_offsets.h"
#include "context.S"
#include "dwarf.S"

.equ STATUS_ACCESS_VIOLATION, 0xc0000005

.section .debug_abbrev, "dr"

.uleb128 1 # entry num
.uleb128 DW_TAG_compile_unit
.byte 1 # has children
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_udata
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_language
.uleb128 DW_FORM_data2
.uleb128 0
.uleb128 0

.uleb128 2 # entry num
.uleb128 DW_TAG_subprogram
.byte 0 # has children
.uleb128 DW_AT_name
.uleb128 DW_FORM_string
.uleb128 DW_AT_low_pc
.uleb128 DW_FORM_addr
.uleb128 DW_AT_high_pc
.uleb128 DW_FORM_data8
.uleb128 0
.uleb128 0

.uleb128 0

.section .debug_info, "dr"

.long .debug_info_end - .debug_info_start

.debug_info_start:
.short 5 # version
.byte DW_UT_compile
.byte 8 # address size
.secrel32 .debug_abbrev

.uleb128 1
.quad .text # low_pc
.uleb128 .text_end - .text # high_pc
.asciz __FILE__ # name
.short DW_LANG_Mips_Assembler # language

dwarf_func_asm init_syscalls
dwarf_func_asm syscall_handler

.uleb128 0

.debug_info_end:

.macro syscall_stub name
    .text
    .global "stub_\name"
    "stub_\name":
        lea rcx, [rip+0f]
        jmp unimplemented_syscall
    0:
        .ascii "\name\0"
    .section .rdata
    .quad "stub_\name"
.endm

.macro syscall_func name, args
    .ifle \args - 4
        .quad "user_\name"
    .else
        .quad "stack_\name"

        .text
        "stack_\name":
            sub rsp, (\args - 4) * 8
            push r9
            push r8
            push rdx
            push rcx

            # ks_addr
            mov rcx, rsp
            add rcx, 0x20
            # us_addr
            mov rdx, gs:GS_CURRENT_THREAD
            add rdx, THREAD_SYSCALL_CONTEXT + CONTEXT_RSP
            mov rdx, [rdx]
            add rdx, 0x28
            # size
            mov r8, (\args - 4) * 8

            # bool copy_from_process(void* ks_addr, const void* us_addr, size_t size);
            sub rsp, 0x20
            call copy_from_process
            add rsp, 0x20

            pop rcx
            pop rdx
            pop r8
            pop r9

            test rax, rax
            jnz 0f

            mov rax, STATUS_ACCESS_VIOLATION
            add rsp, (\args - 4) * 8
            ret

0:
            sub rsp, 0x20
            call "user_\name"
            add rsp, 0x20 + ((\args - 4) * 8)
            ret

        .section .rdata
    .endif
.endm

.section .rdata
syscalls:
#include "syscall-list.S"
end_syscalls:

.section .text

.global init_syscalls
init_syscalls:
    mov ecx, 0xc0000082 # LSTAR
    lea rax, [rip+syscall_handler]
    mov rdx, rax
    shr rdx, 32
    wrmsr

    mov ecx, 0xc0000081 # STAR
    xor eax, eax
    mov edx, 0x002b0008 # ((user CS - 16) << 16) | kernel CS
    wrmsr

    mov ecx, 0xc0000084 # SFMASK
    xor edx, edx
    mov eax, 0x200 # disable interrupts when syscall_handler called
    wrmsr

    ret
init_syscalls_end:

.global syscall_handler
syscall_handler:
    swapgs
    save_syscall_context

    # change to kernel stack
    mov rax, gs:GS_CURRENT_THREAD
    mov rsp, [rax + THREAD_SYSCALL_STACK]

    # mark thread as running kernel code
    mov BYTE PTR [rax + THREAD_KERNEL_MODE], 1

    # load kernel cr3
    mov rax, gs:GS_KERNEL_CR3
    mov cr3, rax

    mov rax, gs:GS_SYSCALL_RAX
    cmp rax, 0x1000
    jl Lno_syscall
    sub rax, 0x1000
    cmp rax, NUM_SYSCALLS
    jge Lno_syscall

    shl rax, 3
    lea rcx, [rip+syscalls]
    add rax, rcx
    mov rax, [rax]

    mov rcx, r10

    sti
    call rax
    cli

Lafter_syscall:
    push rax
    call acquire_sync_lock

    mov rcx, [rsp]
    call maybe_yield

    call release_sync_lock
    pop rax

    # mark thread as running user code
    mov rdx, gs:GS_CURRENT_THREAD
    mov BYTE PTR [rdx + THREAD_KERNEL_MODE], 0

    mov rcx, 0x33 # ss
    push rcx
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RSP]
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_EFLAGS]
    mov rcx, 0x3b # cs
    push rcx
    push [rdx + THREAD_SYSCALL_CONTEXT + CONTEXT_RIP]

    # load process cr3
    mov rcx, [rdx + THREAD_PROC]
    mov rcx, [rcx + PROCESS_CR3]
    mov cr3, rcx

    swapgs

    # don't leak information to userspace
    xor rcx, rcx
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11

    iretq

Lno_syscall:
    mov rax, 0xffffffffc0000002 # STATUS_NOT_IMPLEMENTED
    jmp Lafter_syscall

syscall_handler_end:

.text_end:
