#include <signal.h>
#include <string.h>

import nt;
import print;
import ucrtbase;
import unique_handle;

#undef errno
import errno;

static const int LAST_SIG = SIGTTOU;

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
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    nt_status status;

    if (oldset) {
        uint32_t retlen;

        status = NtQueryInformationThread(NtCurrentThread(),
                                          THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                          oldset, sizeof(*oldset), &retlen);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }
    }

    if (set) {
        switch (how) {
            case SIG_BLOCK:
                status = NtSetInformationThread(NtCurrentThread(),
                                                THREAD_INFORMATION_CLASS::MoxnoxBlockSignals,
                                                set, sizeof(*set));
                if (!success(status)) {
                    *_errno() = nt_status_to_errno(status);
                    return -1;
                }
            break;

            case SIG_UNBLOCK: {
                LARGE_INTEGER timeout;

                status = NtSetInformationThread(NtCurrentThread(),
                                                THREAD_INFORMATION_CLASS::MoxnoxUnblockSignals,
                                                set, sizeof(*set));
                if (!success(status)) {
                    *_errno() = nt_status_to_errno(status);
                    return -1;
                }

                // run APCs

                timeout.QuadPart = 0;

                status = NtDelayExecution(true, &timeout);

                if (!success(status) && status != nt_status::TIMEOUT) {
                    *_errno() = nt_status_to_errno(status);
                    return -1;
                }

                break;
            }

            case SIG_SETMASK: {
                LARGE_INTEGER timeout;

                status = NtSetInformationThread(NtCurrentThread(),
                                                THREAD_INFORMATION_CLASS::MoxnoxSignalsMask,
                                                set, sizeof(*set));
                if (!success(status)) {
                    *_errno() = nt_status_to_errno(status);
                    return -1;
                }

                // run APCs

                timeout.QuadPart = 0;

                status = NtDelayExecution(true, &timeout);

                if (!success(status) && status != nt_status::TIMEOUT) {
                    *_errno() = nt_status_to_errno(status);
                    return -1;
                }

                break;
            }

            default:
                *_errno() = EINVAL;
                return -1;
        }
    }

    return 0;
}

extern "C"
int kill(pid_t pid, int sig) {
    CLIENT_ID client_id;
    OBJECT_ATTRIBUTES oa;
    HANDLE h;
    unique_handle proc;
    nt_status status;

    // FIXME - pid 0 means all processes in this process group
    // FIXME - pid -1 means all processes
    // FIXME - pid < -1 means to process group -pid

    if (pid <= 0) {
        *_errno() = EINVAL;
        return -1;
    }

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

    if (sig == SIGKILL)
        status = NtTerminateProcess(proc.get(), (nt_status)SIGKILL);
    else if (sig == SIGSTOP)
        status = NtSuspendProcess(proc.get());
    else {
        status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxSetPendingSignal,
                                        &sig, sizeof(sig));
    }

    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}
