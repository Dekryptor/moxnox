#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <atomic>
#include <array>
#include <utility>

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

static void default_signal_handler(int sig) {
    switch (sig) {
        // terminate
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:
        case SIGIOT:
        case SIGEMT:
        case SIGFPE:
        case SIGBUS:
        case SIGSEGV:
        case SIGSYS:
        case SIGPIPE:
        case SIGALRM:
        case SIGTERM:
        case SIGPOLL:
        case SIGABRT:
        case SIGXCPU:
        case SIGXFSZ:
        case SIGVTALRM:
        case SIGPROF:
        case SIGUSR1:
        case SIGUSR2:
        case SIGBREAK:
            _exit(sig);
            break;

        // ignore
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
        case SIGCONT:
            return;

        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            NtSuspendProcess(NtCurrentProcess());
            break;

        // no SIGKILL or SIGSTOP

        default:
            print("default_signal_handler: unexpected signal {}, terminating\n", sig);
            _exit(sig);
            break;
    }

    NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
    unreachable();
}

void signal_dispatch(int sig) {
    if (sig <= 0 || sig > LAST_SIG) {
        print("signal_dispatch: invalid signal number {}\n", sig);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    auto handler = sig_handlers[(size_t)(sig - 1)].load();

    if (handler == SIG_IGN)
        return;

    if (handler == SIG_DFL)
        handler = default_signal_handler;

    handler(sig);
}
