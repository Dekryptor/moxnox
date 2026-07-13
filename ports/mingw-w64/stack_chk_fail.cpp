/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

// adapted from mingw to use NT syscalls directly

#include <utility>

using namespace std;

import nt;

extern "C" void __cdecl __attribute__((__noreturn__)) __stack_chk_fail(void);

extern "C"
void __cdecl __attribute__((__noreturn__)) __stack_chk_fail(void) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    static const char msg[] = "*** stack smashing detected ***: terminated\n";

    NtWriteFile(peb.ProcessParameters->StandardError, 0, nullptr, 0, &iosb,
                msg, sizeof(msg) - 1, nullptr, nullptr);

    NtTerminateProcess(NtCurrentProcess(), nt_status::STACK_BUFFER_OVERRUN);

    unreachable();
}
