module;

#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <array>
#include <utility>
#include <optional>

export module except;

import nt;
import ntdll;
import ucrtbase;
import print;

#undef errno
import errno;

using namespace std;

// ntdll
extern "C" void (*mn_signal_dispatch)(int sig) __declspec(dllimport);

extern "C" int* __errno();

static const int LAST_SIG = SIGTTOU;

static RTL_CRITICAL_SECTION sig_handlers_lock;
static array<struct ::sigaction, LAST_SIG> sig_handlers;

extern "C"
int mn_sigaction(int sig, const struct sigaction* act,
                 struct sigaction* oldact) {
    if (sig <= 0 || sig > LAST_SIG) {
        *__errno() = EINVAL;
        return -1;
    }

    RtlEnterCriticalSection(&sig_handlers_lock);

    if (oldact)
        *oldact = sig_handlers[(size_t)(sig - 1)];

    if (act)
        sig_handlers[(size_t)(sig - 1)] = *act;

    RtlLeaveCriticalSection(&sig_handlers_lock);

    return 0;
}

extern "C"
__sighandler_t signal(int sig, __sighandler_t func) {
    struct sigaction sa, old;

    if (sig <= 0 || sig > LAST_SIG)
        return SIG_ERR;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = func;
    sa.sa_flags = SA_RESTART;

    if (mn_sigaction(sig, &sa, &old) != 0)
        return SIG_ERR;

    return old.sa_handler;
}

extern "C"
int raise(int sig) {
    nt_status status;
    LARGE_INTEGER timeout;

    if (sig == SIGKILL) {
        NtTerminateProcess(NtCurrentProcess(), (nt_status)SIGKILL);
        unreachable();
    } else if (sig == SIGSTOP) {
        NtSuspendProcess(NtCurrentProcess());
        return 0;
    }

    status = NtSetInformationThread(NtCurrentThread(),
                                    THREAD_INFORMATION_CLASS::MoxnoxSetPendingSignal,
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
            NtTerminateProcess(NtCurrentProcess(), (nt_status)(0x80000000 | (unsigned int)sig));
            unreachable();

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
            NtTerminateProcess(NtCurrentProcess(), (nt_status)(0x80000000 | (unsigned int)sig));
            unreachable();
    }

    NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
    unreachable();
}

static void signal_dispatch(int sig) {
    if (sig <= 0 || sig > LAST_SIG) {
        print("signal_dispatch: invalid signal number {}\n", sig);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    struct sigaction sa;

    RtlEnterCriticalSection(&sig_handlers_lock);

    sa = sig_handlers[(size_t)(sig - 1)];

    if (sa.sa_flags & (int)SA_RESETHAND)
        memset(&sig_handlers[(size_t)(sig - 1)], 0, sizeof(struct sigaction));

    RtlLeaveCriticalSection(&sig_handlers_lock);

    if (sa.sa_handler == SIG_IGN)
        return;

    auto mask = sa.sa_mask;
    optional<sigset_t> oldset;

    if (!(sa.sa_flags & SA_NODEFER))
        mask |= (sigset_t)1 << (sig - 1);

    if (mask != 0) {
        nt_status status;
        uint32_t retlen;
        sigset_t set;

        status = NtQueryInformationThread(NtCurrentThread(),
                                          THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                          &set, sizeof(set), &retlen);
        if (!success(status)) {
            print_stderr("NtQueryInformationThread for MoxnoxSignalsMask returned {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }

        oldset = set;

        status = NtSetInformationThread(NtCurrentThread(),
                                        THREAD_INFORMATION_CLASS::MoxnoxBlockSignals,
                                        &mask, sizeof(mask));
        if (!success(status)) {
            print_stderr("NtSetInformationThread for MoxnoxBlockSignals returned {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }
    }

    if (sa.sa_handler == SIG_DFL)
        default_signal_handler(sig);
    else if (sa.sa_flags & SA_SIGINFO) {
        siginfo_t info;

        memset(&info, 0, sizeof(info));

        info.si_signo = sig;
        // FIXME - si_errno
        // FIXME - si_code
        // FIXME - si_pid
        // FIXME - si_uid
        // FIXME - si_status
        // FIXME - si_value
        // FIXME - si_addr
        // FIXME - si_band

        // FIXME - context
        sa.sa_sigaction(sig, &info, nullptr);
    } else
        sa.sa_handler(sig);

    if (oldset.has_value()) {
        nt_status status;
        LARGE_INTEGER timeout;
        sigset_t set = *oldset;

        status = NtSetInformationThread(NtCurrentThread(),
                                        THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                        &set, sizeof(set));
        if (!success(status)) {
            print_stderr("NtSetInformationThread for MoxnoxSignalsMask returned {}\n", status);
            NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
            unreachable();
        }

        // run APCs

        timeout.QuadPart = 0;

        NtDelayExecution(true, &timeout);
    }
}

export void init_signals() {
    mn_signal_dispatch = signal_dispatch;

    RtlInitializeCriticalSection(&sig_handlers_lock);
}
