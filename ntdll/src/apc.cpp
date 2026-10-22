// SPDX-License-Identifier: LGPL-2.1-or-later
#include <utility>

import nt;
import print;

using namespace std;

extern "C" {
    void (*mn_signal_dispatch)(int sig) = nullptr;
}

extern "C"
void mn_handle_apc(CONTEXT* ctx, PS_APC_ROUTINE apc_routine, void* apc_arg1,
                   void* apc_arg2, void* apc_arg3) {
    apc_routine(apc_arg1, apc_arg2, apc_arg3);
    NtContinue(ctx, true);
}

extern "C"
void mn_posix_signal(int sig) {
    // FIXME - would this happen? If yes, maybe just hardcode the defaults here
    if (!mn_signal_dispatch) {
        print("mn_posix_signal: mn_signal_dispatch not set\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    mn_signal_dispatch(sig);
}
