#include <setjmp.h>
#include <stddef.h>
#include <utility>
#include "setjmp_offsets.h"

import ucrtbase;
import nt;
import ntdll;

using namespace std;

#ifdef __x86_64__

static_assert(offsetof(_JUMP_BUFFER, Frame) == JUMP_BUFFER_FRAME);
static_assert(offsetof(_JUMP_BUFFER, Rbx) == JUMP_BUFFER_RBX);
static_assert(offsetof(_JUMP_BUFFER, Rsp) == JUMP_BUFFER_RSP);
static_assert(offsetof(_JUMP_BUFFER, Rbp) == JUMP_BUFFER_RBP);
static_assert(offsetof(_JUMP_BUFFER, Rsi) == JUMP_BUFFER_RSI);
static_assert(offsetof(_JUMP_BUFFER, Rdi) == JUMP_BUFFER_RDI);
static_assert(offsetof(_JUMP_BUFFER, R12) == JUMP_BUFFER_R12);
static_assert(offsetof(_JUMP_BUFFER, R13) == JUMP_BUFFER_R13);
static_assert(offsetof(_JUMP_BUFFER, R14) == JUMP_BUFFER_R14);
static_assert(offsetof(_JUMP_BUFFER, R15) == JUMP_BUFFER_R15);
static_assert(offsetof(_JUMP_BUFFER, Rip) == JUMP_BUFFER_RIP);
static_assert(offsetof(_JUMP_BUFFER, MxCsr) == JUMP_BUFFER_MXCSR);
static_assert(offsetof(_JUMP_BUFFER, FpCsr) == JUMP_BUFFER_FPCSR);
static_assert(offsetof(_JUMP_BUFFER, Xmm6) == JUMP_BUFFER_XMM6);
static_assert(offsetof(_JUMP_BUFFER, Xmm7) == JUMP_BUFFER_XMM7);
static_assert(offsetof(_JUMP_BUFFER, Xmm8) == JUMP_BUFFER_XMM8);
static_assert(offsetof(_JUMP_BUFFER, Xmm9) == JUMP_BUFFER_XMM9);
static_assert(offsetof(_JUMP_BUFFER, Xmm10) == JUMP_BUFFER_XMM10);
static_assert(offsetof(_JUMP_BUFFER, Xmm11) == JUMP_BUFFER_XMM11);
static_assert(offsetof(_JUMP_BUFFER, Xmm12) == JUMP_BUFFER_XMM12);
static_assert(offsetof(_JUMP_BUFFER, Xmm13) == JUMP_BUFFER_XMM13);
static_assert(offsetof(_JUMP_BUFFER, Xmm14) == JUMP_BUFFER_XMM14);
static_assert(offsetof(_JUMP_BUFFER, Xmm15) == JUMP_BUFFER_XMM15);

#endif

#ifndef __x86_64__

extern "C"
int _setjmp(jmp_buf /*env*/, void* /*frame*/) {
    // print("FIXME - _setjmp\n");

    return 0;
}

#endif

#ifdef __x86_64__

extern "C"
void longjmp(jmp_buf env, int value) {
    CONTEXT ctx;
    auto jmp = (_JUMP_BUFFER*)env;
    auto ret = (uintptr_t)value;

    if (ret == 0)
        ret = 1;

    if (jmp->Frame == 0) {
        RtlCaptureContext(&ctx);

        ctx.Rax = ret;
        ctx.Rbx = jmp->Rbx;
        ctx.Rsp = jmp->Rsp;
        ctx.Rbp = jmp->Rbp;
        ctx.Rsi = jmp->Rsi;
        ctx.Rdi = jmp->Rdi;
        ctx.R12 = jmp->R12;
        ctx.R13 = jmp->R13;
        ctx.R14 = jmp->R14;
        ctx.R15 = jmp->R15;
        ctx.Rip = jmp->Rip;
        ctx.MxCsr = jmp->MxCsr;
        ctx.FltSave.ControlWord = jmp->FpCsr;
        ctx.Xmm6 = *(M128A*)&jmp->Xmm6;
        ctx.Xmm7 = *(M128A*)&jmp->Xmm7;
        ctx.Xmm8 = *(M128A*)&jmp->Xmm8;
        ctx.Xmm9 = *(M128A*)&jmp->Xmm9;
        ctx.Xmm10 = *(M128A*)&jmp->Xmm10;
        ctx.Xmm11 = *(M128A*)&jmp->Xmm11;
        ctx.Xmm12 = *(M128A*)&jmp->Xmm12;
        ctx.Xmm13 = *(M128A*)&jmp->Xmm13;
        ctx.Xmm14 = *(M128A*)&jmp->Xmm14;
        ctx.Xmm15 = *(M128A*)&jmp->Xmm15;

        NtContinue(&ctx, false);
    } else {
        RtlUnwindEx((void*)jmp->Frame, (void*)jmp->Rip, nullptr, (void*)ret,
                    &ctx, nullptr);
    }

    unreachable();
}

#else

extern "C"
void longjmp(jmp_buf env, int value) {
    __asm("ud2");
}

#endif
