.intel_syntax noprefix

#include "../../kernel/src/context_offsets.h"
#include "exception_record_offsets.h"
#include "../../modules/dwarf.S"

.equ CONTEXT_AMD64, 0x00100000
.equ CONTEXT_AMD64_CONTROL, CONTEXT_AMD64 | 0x0001
.equ CONTEXT_AMD64_INTEGER, CONTEXT_AMD64 | 0x0002
.equ CONTEXT_AMD64_SEGMENTS, CONTEXT_AMD64 | 0x0004
.equ CONTEXT_AMD64_FLOATING_POINT, CONTEXT_AMD64 | 0x0008

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

dwarf_func_asm NtContinue
dwarf_func_asm RtlCaptureContext
dwarf_func_asm RtlRaiseException

.uleb128 0

.debug_info_end:

.text

# NTSTATUS NtContinue(const CONTEXT* thread_context, bool raise_alert);
.seh_proc NtContinue
.global NtContinue
NtContinue:
.seh_endprologue

    # FIXME - raise_alert
    # FIXME - debug registers

    mov ds, WORD PTR [rcx + CONTEXT_DS]
    mov es, WORD PTR [rcx + CONTEXT_DS]
    mov fs, WORD PTR [rcx + CONTEXT_DS]

    fxrstor [rcx + CONTEXT_FXSAVE]

    mov rdx, QWORD PTR [rcx + CONTEXT_RDX]
    mov rbx, QWORD PTR [rcx + CONTEXT_RBX]
    mov rbp, QWORD PTR [rcx + CONTEXT_RBP]
    mov rsi, QWORD PTR [rcx + CONTEXT_RSI]
    mov rdi, QWORD PTR [rcx + CONTEXT_RDI]
    mov r8, QWORD PTR [rcx + CONTEXT_R8]
    mov r9, QWORD PTR [rcx + CONTEXT_R9]
    mov r10, QWORD PTR [rcx + CONTEXT_R10]
    mov r11, QWORD PTR [rcx + CONTEXT_R11]
    mov r12, QWORD PTR [rcx + CONTEXT_R12]
    mov r13, QWORD PTR [rcx + CONTEXT_R13]
    mov r14, QWORD PTR [rcx + CONTEXT_R14]
    mov r15, QWORD PTR [rcx + CONTEXT_R15]

    xor eax, eax
    mov ax, WORD PTR [rcx + CONTEXT_SS]
    push rax
    push QWORD PTR [rcx + CONTEXT_RSP]
    mov eax, DWORD PTR [rcx + CONTEXT_EFLAGS]
    push rax
    xor eax, eax
    mov ax, WORD PTR [rcx + CONTEXT_CS]
    push rax
    push QWORD PTR [rcx + CONTEXT_RIP]

    mov rax, QWORD PTR [rcx + CONTEXT_RAX]
    mov rcx, QWORD PTR [rcx + CONTEXT_RCX]

    iretq
NtContinue_end:
.seh_endproc

# NTSTATUS RtlCaptureContext(CONTEXT* context);
.seh_proc RtlCaptureContext
.global RtlCaptureContext
RtlCaptureContext:
.seh_stackalloc 8
    pushf
.seh_endprologue
    mov DWORD PTR [rcx + CONTEXT_CONTEXT_FLAGS], CONTEXT_AMD64_CONTROL | CONTEXT_AMD64_INTEGER | CONTEXT_AMD64_SEGMENTS | CONTEXT_AMD64_FLOATING_POINT
    stmxcsr DWORD PTR [rcx + CONTEXT_MXCSR] # MxCsr
    mov WORD PTR [rcx + CONTEXT_CS], cs
    mov WORD PTR [rcx + CONTEXT_DS], ds
    mov WORD PTR [rcx + CONTEXT_ES], es
    mov WORD PTR [rcx + CONTEXT_FS], fs
    mov WORD PTR [rcx + CONTEXT_GS], gs
    mov WORD PTR [rcx + CONTEXT_SS], ss
    mov [rcx + CONTEXT_RAX], rax
    mov rax, [rsp]
    mov [rcx + CONTEXT_EFLAGS],rax
    mov [rcx + CONTEXT_RCX], rcx
    mov [rcx + CONTEXT_RDX], rdx
    mov [rcx + CONTEXT_RBX], rbx
    lea rax, [rsp + 0x10]
    mov [rcx + CONTEXT_RSP], rax
    mov [rcx + CONTEXT_RBP], rbp
    mov [rcx + CONTEXT_RSI], rsi
    mov [rcx + CONTEXT_RDI], rdi
    mov [rcx + CONTEXT_R8], r8
    mov [rcx + CONTEXT_R9], r9
    mov [rcx + CONTEXT_R10], r10
    mov [rcx + CONTEXT_R11], r11
    mov [rcx + CONTEXT_R12], r12
    mov [rcx + CONTEXT_R13], r13
    mov [rcx + CONTEXT_R14], r14
    mov [rcx + CONTEXT_R15], r15
    mov rax, [rsp + 0x8]
    mov [rcx + CONTEXT_RIP], rax
    fxsave [rcx + CONTEXT_FXSAVE]

    pop rax
    ret
RtlCaptureContext_end:
.seh_endproc

# void RtlRaiseException(EXCEPTION_RECORD* exception_record);
.seh_proc RtlRaiseException
.global RtlRaiseException
RtlRaiseException:
    sub rsp, 0x4e8
    .seh_stackalloc 0x4e8
    .seh_endprologue

    # stack:
    #   orig_exception_record (rsp + 0x8)
    #   context (rsp + 0x10)
    #   return address (rsp + 0x4e8)
    #   (orig stack address) (rsp + 0x4f0)

    mov [rsp + 0x8], rcx
    lea rcx, [rsp + 0x10]
    call RtlCaptureContext

    # unwind to caller

    lea rdx, [rsp + 0x10]

    lea rax, [rsp + 0x4f0]
    mov [rdx + CONTEXT_RSP], rax

    mov rcx, [rsp + 0x8]
    mov [rdx + CONTEXT_RCX], rcx

    mov rax, QWORD PTR [rsp + 0x4e8]
    mov [rdx + CONTEXT_RIP], rax
    mov [rcx + EXCEPTION_RECORD_EXCEPTION_ADDRESS], rax

    # FIXME - different if Peb->BeingDebugged set?

    call mn_signal
RtlRaiseException_end:
.seh_endproc

.text_end:
