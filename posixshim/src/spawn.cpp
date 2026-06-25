#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <span>

import nt;
import print;
import unique_handle;
import exec;
import list;

#undef errno
import errno;

using namespace std;

extern "C"
int posix_spawn(pid_t* pid, const char* path,
                const posix_spawn_file_actions_t* file_actions,
                const posix_spawnattr_t* /*attrp*/,
                char* const* argv, char* const* envp) {
    unique_handle proc, thread;
    nt_status status;
    CLIENT_ID client_id;

    // FIXME - attrp

    if (!path || path[0] == 0)
        return EINVAL;

    auto ret = do_exec(path, argv, envp, proc, thread, client_id,
                       (file_actions_list*)file_actions);
    if (ret)
        return ret;

    // start thread

    status = NtResumeThread(thread.get(), nullptr);
    if (!success(status)) {
        NtTerminateProcess(proc.get(), status);
        return nt_status_to_errno(status);
    }

    // FIXME - mark process as child

    if (pid)
        *pid = client_id.UniqueProcess;

    return 0;
}

static int exit_code_to_status(uint32_t exit_code) {
    if ((exit_code & 0xc0000000) == 0xc0000000) {
        switch ((nt_status)exit_code) {
            case nt_status::ACCESS_VIOLATION:
                return SIGSEGV;

            case nt_status::INTEGER_DIVIDE_BY_ZERO:
                return SIGFPE; // sic

            case nt_status::ILLEGAL_INSTRUCTION:
                return SIGILL;

            default:
                break;
        }
    }

    if (exit_code & 0x80000000) // killed by signal
        return exit_code & 0x7f;
    else
        return (uint8_t)exit_code << 8;
}

static pid_t wait_for_any_child(int* wstatus, int options) {
    nt_status status;
    HANDLE h;
    uint32_t retlen;
    PROCESS_INFORMATION_CLASS pic;
    PROCESS_BASIC_INFORMATION pbi;
    unique_handle proc;

    if (options & WNOHANG)
        pic = PROCESS_INFORMATION_CLASS::MoxnoxWaitForChildNoHang;
    else
        pic = PROCESS_INFORMATION_CLASS::MoxnoxWaitForChild;

    while (true) {
        status = NtQueryInformationProcess(NtCurrentProcess(), pic, &h,
                                           sizeof(HANDLE), &retlen);
        if (status == nt_status::USER_APC)
            continue;

        break;
    }

    if (status == nt_status::NOT_FOUND) {
        *_errno() = ECHILD;
        return -1;
    } else if (status == nt_status::TIMEOUT)
        return 0;

    proc.reset(h);

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                        &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (wstatus)
        *wstatus = exit_code_to_status((uint32_t)pbi.ExitStatus);

    return (pid_t)pbi.UniqueProcessId;
}

extern "C"
pid_t waitpid(pid_t pid, int* wstatus, int options) {
    nt_status status;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    CLIENT_ID client_id;
    LARGE_INTEGER timeout;
    PROCESS_BASIC_INFORMATION pbi;
    unique_handle proc;

    if (options & WCONTINUED) {
        print("FIXME - waitpid with WCONTINUED\n");
        *_errno() = EINVAL;
        return -1;
    }

    if (options & WUNTRACED) {
        print("FIXME - waitpid with WUNTRACED\n");
        *_errno() = EINVAL;
        return -1;
    }

    if (pid == -1)
        return wait_for_any_child(wstatus, options);
    else if (pid <= 0) {
        print("FIXME - waitpid {}\n", pid); // FIXME
        *_errno() = EINVAL;
        return -1;
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);

    client_id.UniqueProcess = (HANDLE)pid;
    client_id.UniqueThread = 0;

    while (true) {
        bool pid_changed = false;

        status = NtOpenProcess(&h, SYNCHRONIZE, &oa, &client_id);
        if (status == nt_status::INVALID_CID) {
            *_errno() = ECHILD;
            return -1;
        } else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        proc.reset(h);

        status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxIsOurChild,
                                           nullptr, 0, nullptr);
        if (status == nt_status::NOT_FOUND) {
            *_errno() = ECHILD;
            return -1;
        }

        timeout.QuadPart = 0;

        while (true) {
            status = NtWaitForSingleObject(proc.get(), true,
                                           options & WNOHANG ? &timeout : nullptr);
            if (status == nt_status::USER_APC)
                continue;

            auto status2 = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                                     &pbi, sizeof(pbi), nullptr);
            if (!success(status2)) {
                *_errno() = nt_status_to_errno(status2);
                return -1;
            }

            // pid changed because of exec - do the whole thing again
            if ((pid_t)pbi.UniqueProcessId != pid) {
                pid_changed = true;
                break;
            }

            if (status == nt_status::TIMEOUT)
                return 0;

            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            break;
        }

        if (!pid_changed)
            break;
    }

    if (wstatus)
        *wstatus = exit_code_to_status((uint32_t)pbi.ExitStatus);

    NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxReapZombie,
                            nullptr, 0);

    return pid;
}

extern "C"
int getppid() {
    PROCESS_BASIC_INFORMATION pbi;

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESS_INFORMATION_CLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return 0;

    return (int)pbi.InheritedFromUniqueProcessId;
}

extern "C"
int posix_spawn_file_actions_init(posix_spawn_file_actions_t* file_actions) {
    new (file_actions) file_actions_list;

    return 0;
}

extern "C"
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* file_actions) {
    auto& fa = *reinterpret_cast<file_actions_list*>(file_actions);

    while (!is_list_empty(fa.list)) {
        auto& eil = containing_record<exec_info_list>(fa.list.next);

        remove_entry_list(eil.le);

        if (eil.ei.type == exec_info_type::open)
            free(eil.ei.open.path);

        free(&eil);
    }

    return 0;
}

extern "C"
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* file_actions,
                                     int fd, const char* path, int oflag,
                                     mode_t mode) {
    auto& fa = *reinterpret_cast<file_actions_list*>(file_actions);

    auto newpath = strdup(path);
    if (!newpath) {
        *_errno() = ENOMEM;
        return -1;
    }

    auto e = new exec_info_list;
    if (!e) {
        free(newpath);
        *_errno() = ENOMEM;
        return -1;
    }

    e->ei.type = exec_info_type::open;
    e->ei.open.fd = fd;
    e->ei.open.path = newpath;
    e->ei.open.oflag = oflag;
    e->ei.open.mode = mode;

    insert_tail_list(fa.list, e->le);

    return 0;
}

extern "C"
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* file_actions,
                                      int fd) {
    auto& fa = *reinterpret_cast<file_actions_list*>(file_actions);

    auto e = new exec_info_list;
    if (!e) {
        *_errno() = ENOMEM;
        return -1;
    }

    e->ei.type = exec_info_type::close;
    e->ei.close.fd = fd;

    insert_tail_list(fa.list, e->le);

    return 0;
}

extern "C"
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* file_actions,
                                     int fd, int newfd) {
    auto& fa = *reinterpret_cast<file_actions_list*>(file_actions);

    auto e = new exec_info_list;
    if (!e) {
        *_errno() = ENOMEM;
        return -1;
    }

    e->ei.type = exec_info_type::dup2;
    e->ei.dup2.fd = fd;
    e->ei.dup2.newfd = newfd;

    insert_tail_list(fa.list, e->le);

    return 0;
}

extern "C"
int posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t* file_actions,
                                      const char* path) {
    auto& fa = *reinterpret_cast<file_actions_list*>(file_actions);

    auto newpath = strdup(path);
    if (!newpath) {
        *_errno() = ENOMEM;
        return -1;
    }

    auto e = new exec_info_list;
    if (!e) {
        free(newpath);
        *_errno() = ENOMEM;
        return -1;
    }

    e->ei.type = exec_info_type::chdir;
    e->ei.chdir.path = newpath;

    insert_tail_list(fa.list, e->le);

    return 0;
}

extern "C"
int posix_spawn_file_actions_addfchdir(posix_spawn_file_actions_t* file_actions,
                                       int fd) {
    auto& fa = *reinterpret_cast<file_actions_list*>(file_actions);

    auto e = new exec_info_list;
    if (!e) {
        *_errno() = ENOMEM;
        return -1;
    }

    e->ei.type = exec_info_type::fchdir;
    e->ei.fchdir.fd = fd;

    insert_tail_list(fa.list, e->le);

    return 0;
}
