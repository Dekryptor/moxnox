#include <signal.h>

import nt;
import print;
import ucrtbase;

#undef errno
import errno;

static const int LAST_SIG = SIGTTOU;

extern "C"
int sigaction(int /*signum*/, const struct sigaction* /*act*/,
              struct sigaction* /*oldact*/) {
    // FIXME

    return 0;
}

extern "C"
int sigemptyset(sigset_t* set) {
    *set = 0;

    return 0;
}

extern "C"
int sigfillset(sigset_t* set) {
    *set = ~(sigset_t)0;

    return 0;
}

extern "C"
int sigaddset(sigset_t* set, int signum) {
    if (signum == 0 || signum > LAST_SIG) {
        *_errno() = EINVAL;
        return -1;
    }

    *set |= (sigset_t)1 << (signum - 1);

    return 0;
}

extern "C"
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    nt_status status;

    if (oldset) {
        // FIXME - oldset
    }

    if (set) {
        switch (how) {
            case SIG_BLOCK:
                status = NtSetInformationThread(NtCurrentThread(),
                                                THREADINFOCLASS::MoxnoxBlockSignals,
                                                set, sizeof(*set));
                if (!success(status)) {
                    *_errno() = nt_status_to_errno(status);
                    return -1;
                }
            break;

            // FIXME - SIG_UNLOCK, SIG_SETMASK

            default:
                *_errno() = EINVAL;
                return -1;
        }
    }

    return 0;
}
