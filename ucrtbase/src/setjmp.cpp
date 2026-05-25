#include <setjmp.h>
#include <stddef.h>
#include "setjmp_offsets.h"

import ucrtbase;
import print;

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
