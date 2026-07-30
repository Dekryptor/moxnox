#include <signal.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>

import nt;
import print;
import ucrtbase;
import unique_handle;

#undef errno
import errno;

static const int LAST_SIG = SIGTTOU;

static char signal_string[10];

// ucrtbase
extern "C" int mn_sigaction(int signum, const struct sigaction* act,
                            struct sigaction* oldact);

extern "C"
int sigaction(int signum, const struct sigaction* act,
              struct sigaction* oldact) {
    return mn_sigaction(signum, act, oldact);
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
int sigdelset(sigset_t* set, int signum) {
    if (signum == 0 || signum > LAST_SIG) {
        *_errno() = EINVAL;
        return -1;
    }

    *set &= ~((sigset_t)1 << (signum - 1));

    return 0;
}

extern "C"
int sigismember(const sigset_t* set, int signo) {
    if (signo == 0 || signo > LAST_SIG)
        return 0;

    return (*set & (sigset_t)1 << (signo - 1)) ? 1 : 0;
}

extern "C"
int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
    nt_status status;

    if (oldset) {
        uint32_t retlen;

        status = NtQueryInformationThread(NtCurrentThread(),
                                          THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                          oldset, sizeof(*oldset), &retlen);
        if (!success(status))
            return nt_status_to_errno(status);
    }

    if (set) {
        switch (how) {
            case SIG_BLOCK:
                status = NtSetInformationThread(NtCurrentThread(),
                                                THREAD_INFORMATION_CLASS::MoxnoxBlockSignals,
                                                set, sizeof(*set));
                if (!success(status))
                    return nt_status_to_errno(status);
            break;

            case SIG_UNBLOCK: {
                LARGE_INTEGER timeout;

                status = NtSetInformationThread(NtCurrentThread(),
                                                THREAD_INFORMATION_CLASS::MoxnoxUnblockSignals,
                                                set, sizeof(*set));
                if (!success(status))
                    return nt_status_to_errno(status);

                // run APCs

                timeout.QuadPart = 0;

                status = NtDelayExecution(true, &timeout);

                if (!success(status) && status != nt_status::TIMEOUT)
                    return nt_status_to_errno(status);

                break;
            }

            case SIG_SETMASK: {
                LARGE_INTEGER timeout;

                status = NtSetInformationThread(NtCurrentThread(),
                                                THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                                set, sizeof(*set));
                if (!success(status))
                    return nt_status_to_errno(status);

                // run APCs

                timeout.QuadPart = 0;

                status = NtDelayExecution(true, &timeout);

                if (!success(status) && status != nt_status::TIMEOUT)
                    return nt_status_to_errno(status);

                break;
            }

            default:
                return EINVAL;
        }
    }

    return 0;
}

extern "C"
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    auto ret = pthread_sigmask(how, set, oldset);

    if (!ret)
        return 0;

    *_errno() = ret;
    return -1;
}

extern "C"
int kill(pid_t pid, int sig) {
    nt_status status;

    // FIXME - sig == 0 means check validity of pid

    if (pid == -1) {
        // FIXME - all processes
        *_errno() = EINVAL;
        return -1;
    }

    if (pid <= 0) {
        MN_PENDING_SIGNAL_PROCESS_GROUP params;

        params.sig = sig;

        if (pid == 0) { // all processes in this process group
            auto pgid = getpgid(0);

            if (pgid == -1)
                return -1;

            params.pgid = (uintptr_t)pgid;
        } else
            params.pgid = (uintptr_t)-pid;

        status = NtSetInformationProcess(NtCurrentProcess(),
                                         PROCESS_INFORMATION_CLASS::MoxnoxSetPendingSignalProcessGroup,
                                         &params, sizeof(params));
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }
    } else {
        CLIENT_ID client_id;
        OBJECT_ATTRIBUTES oa;
        HANDLE h;
        unique_handle proc;

        client_id.UniqueProcess = (HANDLE)pid;
        client_id.UniqueThread = 0;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        status = NtOpenProcess(&h, PROCESS_TERMINATE, &oa, &client_id);
        if (status == nt_status::INVALID_CID) {
            *_errno() = ESRCH;
            return -1;
        } else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        proc.reset(h);

        status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxSetPendingSignal,
                                        &sig, sizeof(sig));
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }
    }

    return 0;
}

extern "C"
[[noreturn]] void siglongjmp(sigjmp_buf env, int val) {
    if (!(env->sig & (sigset_t)1 << 63))
        sigprocmask(SIG_SETMASK, &env->sig, nullptr);

    longjmp(env->jmp, val);
}

extern "C"
int pause() {
    auto count = mn_handled_signal_count();

    while (count == mn_handled_signal_count()) {
        NtDelayExecution(true, nullptr);
    }

    *_errno() = EINTR;
    return -1;
}

extern "C"
char* strsignal(int sig) {
    switch (sig) {
        case SIGHUP:
            strcpy(signal_string, "SIGHUP");
            break;
        case SIGINT:
            strcpy(signal_string, "SIGINT");
            break;
        case SIGQUIT:
            strcpy(signal_string, "SIGQUIT");
            break;
        case SIGILL:
            strcpy(signal_string, "SIGILL");
            break;
        case SIGTRAP:
            strcpy(signal_string, "SIGTRAP");
            break;
        case SIGIOT:
            strcpy(signal_string, "SIGIOT");
            break;
        case SIGEMT:
            strcpy(signal_string, "SIGEMT");
            break;
        case SIGFPE:
            strcpy(signal_string, "SIGFPE");
            break;
        case SIGKILL:
            strcpy(signal_string, "SIGKILL");
            break;
        case SIGBUS:
            strcpy(signal_string, "SIGBUS");
            break;
        case SIGSEGV:
            strcpy(signal_string, "SIGSEGV");
            break;
        case SIGSYS:
            strcpy(signal_string, "SIGSYS");
            break;
        case SIGPIPE:
            strcpy(signal_string, "SIGPIPE");
            break;
        case SIGALRM:
            strcpy(signal_string, "SIGALRM");
            break;
        case SIGTERM:
            strcpy(signal_string, "SIGTERM");
            break;
        case SIGPOLL:
            strcpy(signal_string, "SIGPOLL");
            break;
        case SIGCHLD:
            strcpy(signal_string, "SIGCHLD");
            break;
        case SIGCONT:
            strcpy(signal_string, "SIGCONT");
            break;
        case SIGSTOP:
            strcpy(signal_string, "SIGSTOP");
            break;
        case SIGTSTP:
            strcpy(signal_string, "SIGTSTP");
            break;
        case SIGBREAK:
            strcpy(signal_string, "SIGBREAK");
            break;
        case SIGABRT:
            strcpy(signal_string, "SIGABRT");
            break;
        case SIGURG:
            strcpy(signal_string, "SIGURG");
            break;
        case SIGXCPU:
            strcpy(signal_string, "SIGXCPU");
            break;
        case SIGXFSZ:
            strcpy(signal_string, "SIGXFSZ");
            break;
        case SIGVTALRM:
            strcpy(signal_string, "SIGVTALRM");
            break;
        case SIGPROF:
            strcpy(signal_string, "SIGPROF");
            break;
        case SIGWINCH:
            strcpy(signal_string, "SIGWINCH");
            break;
        case SIGUSR1:
            strcpy(signal_string, "SIGUSR1");
            break;
        case SIGUSR2:
            strcpy(signal_string, "SIGUSR2");
            break;
        case SIGTTIN:
            strcpy(signal_string, "SIGTTIN");
            break;
        case SIGTTOU:
            strcpy(signal_string, "SIGTTOU");
            break;
        default:
            strcpy(signal_string, "?");
            break;
    }

    return signal_string;
}

extern "C"
int sigwait(const sigset_t* set, int* sig) {
    while (true) {
        auto status = NtSetInformationThread(NtCurrentThread(),
                                             THREAD_INFORMATION_CLASS::MoxnoxWaitForSignal,
                                             set, sizeof(sigset_t));
        if (!success(status))
            return nt_status_to_errno(status);

        if (status == nt_status::ALERTED || status == nt_status::USER_APC)
            continue;

        *sig = (int)status;

        break;
    }

    return 0;
}
