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

#elifdef __aarch64__

static_assert(offsetof(_JUMP_BUFFER, Frame) == JUMP_BUFFER_FRAME);
static_assert(offsetof(_JUMP_BUFFER, X19) == JUMP_BUFFER_X19);
static_assert(offsetof(_JUMP_BUFFER, X20) == JUMP_BUFFER_X20);
static_assert(offsetof(_JUMP_BUFFER, X21) == JUMP_BUFFER_X21);
static_assert(offsetof(_JUMP_BUFFER, X22) == JUMP_BUFFER_X22);
static_assert(offsetof(_JUMP_BUFFER, X23) == JUMP_BUFFER_X23);
static_assert(offsetof(_JUMP_BUFFER, X24) == JUMP_BUFFER_X24);
static_assert(offsetof(_JUMP_BUFFER, X25) == JUMP_BUFFER_X25);
static_assert(offsetof(_JUMP_BUFFER, X26) == JUMP_BUFFER_X26);
static_assert(offsetof(_JUMP_BUFFER, X27) == JUMP_BUFFER_X27);
static_assert(offsetof(_JUMP_BUFFER, X28) == JUMP_BUFFER_X28);
static_assert(offsetof(_JUMP_BUFFER, Fp) == JUMP_BUFFER_FP);
static_assert(offsetof(_JUMP_BUFFER, Lr) == JUMP_BUFFER_LR);
static_assert(offsetof(_JUMP_BUFFER, Sp) == JUMP_BUFFER_SP);
static_assert(offsetof(_JUMP_BUFFER, Fpcr) == JUMP_BUFFER_FPCR);
static_assert(offsetof(_JUMP_BUFFER, Fpsr) == JUMP_BUFFER_FPSR);
static_assert(offsetof(_JUMP_BUFFER, D) == JUMP_BUFFER_D);

#endif

extern "C"
void _longjmp(jmp_buf env, int value) {
    CONTEXT ctx;
    auto jmp = (_JUMP_BUFFER*)env;
    auto ret = (uintptr_t)value;

    if (ret == 0)
        ret = 1;

    if (jmp->Frame == 0) {
        RtlCaptureContext(&ctx);

#ifdef __x86_64__
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
#elifdef __aarch64__
        ctx.X0 = ret;
        ctx.X19 = jmp->X19;
        ctx.X20 = jmp->X20;
        ctx.X21 = jmp->X21;
        ctx.X22 = jmp->X22;
        ctx.X23 = jmp->X23;
        ctx.X24 = jmp->X24;
        ctx.X25 = jmp->X25;
        ctx.X26 = jmp->X26;
        ctx.X27 = jmp->X27;
        ctx.X28 = jmp->X28;
        ctx.Fp = jmp->Fp;
        ctx.Pc = jmp->Lr;
        ctx.Sp = jmp->Sp;
        ctx.Fpcr = jmp->Fpcr;
        ctx.Fpsr = jmp->Fpsr;
        ctx.V[8].D[0] = jmp->D[0];
        ctx.V[9].D[0] = jmp->D[1];
        ctx.V[10].D[0] = jmp->D[2];
        ctx.V[11].D[0] = jmp->D[3];
        ctx.V[12].D[0] = jmp->D[4];
        ctx.V[13].D[0] = jmp->D[5];
        ctx.V[14].D[0] = jmp->D[6];
        ctx.V[15].D[0] = jmp->D[7];
#endif

        NtContinue(&ctx, false);
    } else {
#ifdef __x86_64__
        RtlUnwindEx((void*)jmp->Frame, (void*)jmp->Rip, nullptr, (void*)ret,
                    &ctx, nullptr);
#elifdef __aarch64__
        RtlUnwindEx((void*)jmp->Frame, (void*)jmp->Lr, nullptr, (void*)ret,
                    &ctx, nullptr);
#endif
    }

    unreachable();
}
