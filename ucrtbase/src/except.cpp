#include <stdint.h>
#include <signal.h>
#include <atomic>
#include <array>

import nt;
import ucrtbase;
import print;

#undef errno
import errno;

using namespace std;

extern "C" int* __errno();

static const int LAST_SIG = SIGTTOU;

static array<atomic<__sighandler_t>, LAST_SIG> sig_handlers;

extern "C"
__sighandler_t signal(int sig, __sighandler_t func) {
    if (sig <= 0 || sig > LAST_SIG)
        return SIG_ERR;

    return sig_handlers[(size_t)(sig - 1)].exchange(func);
}

extern "C"
int raise(int sig) {
    nt_status status;
    LARGE_INTEGER timeout;

    status = NtSetInformationThread(NtCurrentThread(), THREADINFOCLASS::MoxnoxSetPendingSignal,
                                    &sig, sizeof(sig));
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return 1;
    }

    // make it so APCs run

    timeout.QuadPart = 0;

    status = NtDelayExecution(true, &timeout);

    if (!success(status) && status != nt_status::TIMEOUT) {
        *__errno() = nt_status_to_errno(status);
        return 1;
    }

    return 0;
}
