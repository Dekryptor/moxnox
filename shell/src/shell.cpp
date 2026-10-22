// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#include <getopt.h>
#include <glob.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <span>
#include <charconv>
#include <chrono>
#include <utility>
#include <string>

using namespace std;

import nt;
import ntdll;
import format;
import print;
import unicode;
import unique_handle;
import list;
import kernelbase;
import pagesize;
import cd;
import termios;
import moxnox_ioctls;
import win32error;

#undef errno
import errno;

static HANDLE running_proc = 0;

static char* prompt_buf;
static string_view prompt;

static HANDLE info_stream = 0;

static sigjmp_buf sigint_jmpbuf;

static char* history_file;

struct suspended_process {
    list_entry le;
    unique_handle proc;
    termios tcsettings;
};

static list_entry suspended_processes = { &suspended_processes, &suspended_processes };
static unsigned int num_suspended_processes = 0;

static unique_handle sigterm_event;
static bool received_sigterm;

void* operator new(size_t count) {
    return malloc(count);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

static void run_program(unique_handle& proc, termios* prev_tc_settings) {
    nt_status status;
    bool done_suspend = false;

    do {
        array<HANDLE, 3> objs;

        objs[0] = sigterm_event.get();
        objs[1] = proc.get();
        objs[2] = info_stream;

        status = NtWaitForMultipleObjects(info_stream == 0 ? 2 : 3,
                                          objs.data(), true, true, nullptr);

        if (status == nt_status::ALERTED || status == nt_status::USER_APC)
            continue;

        if (!success(status)) {
            print_stderr("NtWaitForMultipleObjects returned {}\n", status);
            return;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: // shell received SIGTERM
            case 1: // process finished
                running_proc = 0;
                break;

            case 2: { // pty event
                IO_STATUS_BLOCK iosb;
                pty_info_event ev;

                status = NtReadFile(info_stream, 0, nullptr, nullptr, &iosb,
                                    &ev, sizeof(ev), nullptr, nullptr);

                if (!success(status)) {
                    print_stderr("NtReadFile returned {}\n", status);
                    continue;
                }

                if (ev.type == pty_info_type::suspend) {
                    print("^Z\n");
                    // FIXME - bash also prints command line
                    print("[{}]+  Stopped\n", num_suspended_processes + 1);

                    auto sp = new suspended_process;
                    if (!sp) {
                        print("new failed\n");
                        return;
                    }

                    swap(sp->proc, proc);

                    if (tcgetattr(STDOUT_FILENO, &sp->tcsettings))
                        print("tcgetattr failed\n");

                    insert_tail_list(suspended_processes, sp->le);

                    num_suspended_processes++;
                    running_proc = 0;
                    done_suspend = true;
                }

                break;
            }
        }
    } while (running_proc != 0);

    if (prev_tc_settings)
        tcsetattr(STDOUT_FILENO, TCSANOW, prev_tc_settings);

    PROCESS_BASIC_INFORMATION pbi;

    if (!done_suspend) {
        status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                        &pbi, sizeof(pbi), nullptr);
        if (!success(status))
            print_stderr("NtQueryInformationProcess returned {}\n", status);
        else if (pbi.ExitStatus == nt_status::CONTROL_C_EXIT)
            print("^C\n");
    }

    print("\e[?25h" // re-enable cursor
        "\e[0m" // reset colours and attributes
        "\e[?1000l" // disable mouse tracking
        "\e[?2004l" // disable bracketed paste mode
        "\e>\e[?1l" // reset application keypad and cursor key modes
        "\e7\e[r\e8" // reset scrolling region
    );
}

static constexpr chrono::year_month_day system_time_to_ymd(int64_t system_time) noexcept {
    signed long long j, e, f, g, h;
    uint8_t day, month;
    uint16_t year;

    auto num = (int)(system_time / (int64_t)864000000000);

    j = num + 2305814;

    f = (4 * j) + 274277;
    f /= 146097;
    f *= 3;
    f /= 4;
    f += j;
    f += 1363;

    e = (4 * f) + 3;
    g = (e % 1461) / 4;
    h = (5 * g) + 2;

    day = (uint8_t)(((h % 153) / 5) + 1);
    month = (uint8_t)(((h / 153) + 2) % 12 + 1);
    year = static_cast<uint16_t>((e / 1461) - 4716 + ((14 - month) / 12));

    return {chrono::year{year}, chrono::month{month}, chrono::day{day}};
}

static constexpr chrono::hh_mm_ss<chrono::seconds> system_time_to_hms(int64_t system_time) noexcept {
    auto secs = (unsigned int)((system_time % (int64_t)864000000000) / 10000000);

    return chrono::hh_mm_ss{chrono::seconds{secs}};
}

static void time() {
    LARGE_INTEGER time;
    auto status = NtQuerySystemTime(&time);

    if (!success(status)) {
        print_stderr("NtQuerySystemTime returned {}\n", status);
        return;
    }

    print("System time is {}\n", time.QuadPart);

    auto ymd = system_time_to_ymd(time.QuadPart);
    auto hms = system_time_to_hms(time.QuadPart);

    print("{} {}\n", ymd, hms);
}

static void timer() {
    HANDLE h;
    NTSTATUS status;
    LARGE_INTEGER due_time;

    status = NtCreateTimer(&h, TIMER_ALL_ACCESS, nullptr, TIMER_TYPE::SynchronizationTimer);
    if (!success(status)) {
        print_stderr("NtCreateTimer returned {}\n", status);
        return;
    }

    due_time.QuadPart = -10000000; // 1 second

    status = NtSetTimer(h, &due_time, nullptr, nullptr, false,
                        1000, nullptr);
    if (!success(status)) {
        print_stderr("NtSetTimer returned {}\n", status);
        NtClose(h);
        return;
    }

    for (unsigned int i = 0; i < 3; i++) {
        status = NtWaitForSingleObject(h, false, nullptr);
        if (!success(status)) {
            print_stderr("NtWaitForSingleObject returned {}\n", status);
            NtClose(h);
            return;
        }

        print("timer fired\n");
    }

    NtClose(h);
}

static void segfault() {
    *(volatile uint8_t*)0 = 0;
}

inline __attribute__((always_inline)) void inline3() {
    __asm __volatile("nop");
    segfault();
    __asm __volatile("nop");
}

inline __attribute__((always_inline)) void inline2() {
    __asm __volatile("nop");
    inline3();
    __asm __volatile("nop");
}

static void __attribute__((noinline)) inline1() {
    __asm __volatile("nop");
    inline2();
    __asm __volatile("nop");
}

static bool set_pty_fg_proc(HANDLE client_handle, uintptr_t pgid) {
    nt_status status;
    IO_STATUS_BLOCK iosb;

    status = NtDeviceIoControlFile(client_handle, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP,
                                   &pgid, sizeof(pgid), nullptr, 0);
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP returned {}\n", status);
        return false;
    }

    return true;
}

static bool munge_cmdline(string_view cmdline, char16_t*& retptr) {
    bool bs = false, quote = false;
    size_t param_start = 0;
    size_t num_params = 0;
    string_view* params = nullptr;

    for (size_t i = 0; i < cmdline.size(); i++) {
        auto c = cmdline[i];

        if (bs)
            bs = false;
        else if (c == '\\') {
            bs = true;
            continue;
        } else if (c == '"') {
            if (quote) {
                num_params++;
                params = (string_view*)realloc(params, num_params * sizeof(string_view));
                if (!params) {
                    print_stderr("realloc failed\n");
                    return false;
                }

                params[num_params - 1] = string_view(cmdline.data() + param_start,
                                                     i - param_start + 1);

                quote = false;
                param_start = i + 1;
            } else {
                quote = true;
                param_start = i;
            }
        } else if (c == ' ' && !quote) {
            if (i != param_start) {
                num_params++;
                params = (string_view*)realloc(params, num_params * sizeof(string_view));
                if (!params) {
                    print_stderr("realloc failed\n");
                    return false;
                }

                params[num_params - 1] = string_view(cmdline.data() + param_start,
                                                     i - param_start);
            }

            param_start = i + 1;
        }
    }

    if (quote) {
        print_stderr("error: unterminated quote\n");

        // FIXME - bash lets you carry on typing here

        if (params)
            free(params);

        return false;
    }

    if (param_start != cmdline.size()) {
        num_params++;
        params = (string_view*)realloc(params, num_params * sizeof(string_view));
        if (!params) {
            print_stderr("realloc failed\n");
            return false;
        }

        params[num_params - 1] = string_view(cmdline.data() + param_start,
                                             cmdline.size() - param_start);
    }

    char16_t* u16cmdline = nullptr;
    size_t u16len = 0;

    for (size_t i = 0; i < num_params; i++) {
        int ret;
        glob_t g;

        if (i != 0 && params[i][0] != '"') {
            auto s = (char*)malloc(params[i].size() + 1);
            if (!s) {
                print_stderr("malloc failed\n");
                free(params);

                if (u16cmdline)
                    free(u16cmdline);

                return false;
            }

            memcpy(s, params[i].data(), params[i].size());
            s[params[i].size()] = 0;

            ret = glob(s, 0, nullptr, &g);

            free(s);
        } else
            ret = GLOB_NOMATCH;

        if (ret != 0 && ret != GLOB_NOMATCH) {
            print_stderr("glob returned {}\n", ret);
            free(params);

            if (u16cmdline)
                free(u16cmdline);

            return false;
        }

        auto add_param = [&u16cmdline, &u16len](string_view param) {
            auto param_len = utf8_to_utf16_len(param);

            auto newlen = u16len + param_len;

            if (u16len != 0)
                newlen++;

            u16cmdline = (char16_t*)realloc(u16cmdline, newlen * sizeof(char16_t));
            if (!u16cmdline) {
                print_stderr("realloc failed\n");
                return false;
            }

            if (u16len != 0) {
                u16cmdline[u16len] = ' ';
                u16len++;
            }

            utf8_to_utf16(span(u16cmdline + u16len, param_len), param);
            u16len = newlen;

            return true;
        };

        if (!ret) {
            // FIXME - will need to put name in quotes if it contains a space
            // FIXME - what if it contains a double quote - escape it?
            for (size_t i = 0; i < g.gl_pathc; i++) {
                if (!add_param(g.gl_pathv[i])) {
                    globfree(&g);
                    free(params);

                    if (u16cmdline)
                        free(u16cmdline);

                    return false;
                }
            }

            globfree(&g);
        } else {
            if (!add_param(params[i])) {
                free(params);

                if (u16cmdline)
                    free(u16cmdline);

                return false;
            }
        }
    }

    if (params)
        free(params);

    u16cmdline = (char16_t*)realloc(u16cmdline, (u16len + 1) * sizeof(char16_t));
    if (!u16cmdline) {
        print_stderr("realloc failed\n");
        return false;
    }

    u16cmdline[u16len] = 0;

    retptr = u16cmdline;

    return true;
}

static void try_run_program(string_view program, string_view cmdline) {
    termios tc_settings;
    bool tc_settings_valid;
    char16_t* u16cmdline;
    uintptr_t pid;
    nt_status status;
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!munge_cmdline(cmdline, u16cmdline))
        return;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    auto proglen = utf8_to_utf16_len(program);

    unique_handle proc;

    if (program.find('/') != string_view::npos) {
        auto exe = (char16_t*)malloc((proglen + 1) * sizeof(char16_t));
        if (!exe) {
            print_stderr("malloc failed\n");
            free(u16cmdline);
            return;
        }

        utf8_to_utf16(span(exe, proglen), program);
        exe[proglen] = 0;

        if (!CreateProcessW(exe, u16cmdline, nullptr, nullptr, false,
                            CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
            auto le = GetLastError();

            switch (le) {
                case ERROR_FILE_NOT_FOUND:
                    print_stderr("{}: No such file or directory\n", program);
                    break;

                case ERROR_ACCESS_DENIED:
                    print_stderr("{}: Permission denied\n", program);
                    break;

                case ERROR_BAD_EXE_FORMAT:
                    print_stderr("{}: not an executable\n", program);
                    break;

                case ERROR_EXE_MACHINE_TYPE_MISMATCH:
                    print_stderr("{}: unsupported architecture\n", program);
                    break;

                default:
                    print_stderr("CreateProcessW failed (error {})\n", GetLastError());
            }

            free(exe);
            free(u16cmdline);
            return;
        }

        pid = pi.dwProcessId;
        proc.reset(pi.hProcess);

        free(exe);
    } else {
        char16_t* buf = nullptr;
        u16string_view env_path;

        auto path = getenv("PATH");

        if (path && path[0]) {
            auto len = utf8_to_utf16_len(path);

            buf = (char16_t*)malloc(len * sizeof(char16_t));
            if (!buf) {
                print_stderr("malloc failed\n");
                free(u16cmdline);
                return;
            }

            utf8_to_utf16(span(buf, len), path);

            env_path = u16string_view(buf, len);
        }

        while (true) {
            auto col = env_path.find(u':');
            u16string_view dir;

            if (col == u16string_view::npos)
                dir = env_path;
            else
                dir = u16string_view(env_path.data(), col);

            for (unsigned int i = 0; i < 2; i++) {
                if (i == 1 && program.find('.') != string_view::npos)
                    break;

                static const u16string_view suffix = u".exe";

                bool add_suffix = i == 1;

                auto exe = (char16_t*)malloc((dir.size() + 1 + proglen + (add_suffix ? suffix.size() : 0) + 1) * sizeof(char16_t));
                if (!exe) {
                    print_stderr("malloc failed\n");
                    free(u16cmdline);

                    if (buf)
                        free(buf);

                    return;
                }

                memcpy(exe, dir.data(), dir.size() * sizeof(char16_t));
                exe[dir.size()] = '/';
                utf8_to_utf16(span(exe + dir.size() + 1, proglen), program);

                if (add_suffix) {
                    memcpy(exe + dir.size() + 1 + proglen, suffix.data(),
                           suffix.size() * sizeof(char16_t));
                    exe[dir.size() + 1 + proglen + suffix.size()] = 0;
                } else
                    exe[dir.size() + 1 + proglen] = 0;

                if (!CreateProcessW(exe, u16cmdline, nullptr, nullptr, false,
                                    CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
                    auto le = GetLastError();

                    if (le != ERROR_FILE_NOT_FOUND) {
                        switch (le) {
                            case ERROR_ACCESS_DENIED:
                                print_stderr("{}: Permission denied\n", program);
                                break;

                            case ERROR_BAD_EXE_FORMAT:
                                print_stderr("{}: not an executable\n", program);
                                break;

                            case ERROR_EXE_MACHINE_TYPE_MISMATCH:
                                print_stderr("{}: unsupported architecture\n", program);
                                break;

                            default:
                                print_stderr("CreateProcessW failed (error {})\n", GetLastError());
                        }

                        free(exe);
                        free(u16cmdline);

                        if (buf)
                            free(buf);

                        return;
                    }
                } else {
                    pid = pi.dwProcessId;
                    proc.reset(pi.hProcess);
                    break;
                }
            }

            if (proc)
                break;

            if (col == u16string_view::npos)
                break;

            env_path = u16string_view(env_path.data() + col + 1,
                                      env_path.size() - col - 1);
        }

        if (buf)
            free(buf);

        if (!proc) {
            print_stderr("{}: No such file or directory\n", program);
            free(u16cmdline);
            return;
        }
    }

    free(u16cmdline);

    running_proc = proc.get();

    if (!tcgetattr(STDOUT_FILENO, &tc_settings))
        tc_settings_valid = true;

    status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxSetProcessGroup,
                                     &pid, sizeof(pid));
    if (!success(status)) {
        print_stderr("NtSetInformationProcess returned {} for MoxnoxSetProcessGroup\n", status);
        running_proc = 0;
        NtClose(pi.hThread);
        NtTerminateProcess(proc.get(), nt_status::SUCCESS);
        return;
    }

    auto is_tty = isatty(STDOUT_FILENO);

    if (is_tty)
        set_pty_fg_proc(peb.ProcessParameters->StandardOutput, pid);

    status = NtResumeThread(pi.hThread, nullptr);
    if (!success(status)) {
        print_stderr("NtResumeThread returned {}\n", status);
        running_proc = 0;
        NtClose(pi.hThread);

        if (is_tty)
            set_pty_fg_proc(peb.ProcessParameters->StandardOutput, (uintptr_t)getpgid(0));

        return;
    }

    NtClose(pi.hThread);

    run_program(proc, tc_settings_valid ? &tc_settings : nullptr);

    if (is_tty)
        set_pty_fg_proc(peb.ProcessParameters->StandardOutput, (uintptr_t)getpgid(0));
}

static void int3() {
#ifdef __aarch64__
    __asm("brk #1337");
#elifdef __x86_64__
    __asm("int 3");
#endif
}

#ifdef __x86_64__
static void int1() {
    __asm __volatile(
        "pushf\n"
        "\tor QWORD PTR [rsp], 0x100\n"
        "\tpopf"
    );
}

static void general_protection_fault() {
    *(uint8_t*)0x123456789abcdef0 = 1;
}
#endif

static void illegal_instruction() {
#ifdef __aarch64__
    __asm("udf #1337");
#elifdef __x86_64__
    __asm("ud2");
#endif
}

static void divide_by_zero() {
#ifdef __aarch64__
    // no exception on aarch64
    __asm __volatile(
        "udiv x0, x0, xzr"
        :
        :
        : "x0"
    );
#elifdef __x86_64__
    __asm __volatile(
        "xor rax, rax\n"
        "idiv rax\n"
    );
#endif
}

static void scroll(string_view param) {
    int val;

    if (param.empty()) {
        print("usage: scroll <lines>\n");
        return;
    }

    auto [ptr, ec] = from_chars(param.data(), param.data() + param.size(), val);

    if (ptr != param.data() + param.size()) {
        print_stderr("unable to parse \"{}\" as integer\n", param);
        return;
    }

    if (val < 0)
        print("\x1b[{}S", -val);
    else
        print("\x1b[{}T", val);
}

static void cursor() {
    HANDLE h;
    NTSTATUS status;
    LARGE_INTEGER due_time;

    status = NtCreateTimer(&h, TIMER_ALL_ACCESS, nullptr, TIMER_TYPE::SynchronizationTimer);
    if (!success(status)) {
        print_stderr("NtCreateTimer returned {}\n", status);
        return;
    }

    print("\e[?25h");

    due_time.QuadPart = -10000000; // 1 second

    status = NtSetTimer(h, &due_time, nullptr, nullptr, false,
                        1000, nullptr);
    if (!success(status)) {
        print_stderr("NtSetTimer returned {}\n", status);
        NtClose(h);
        return;
    }

    for (unsigned int i = 0; i < 4; i++) {
        status = NtWaitForSingleObject(h, false, nullptr);
        if (!success(status)) {
            print_stderr("NtWaitForSingleObject returned {}\n", status);
            NtClose(h);
            return;
        }

        if (i % 2)
            print("\e[?25h");
        else
            print("\e[?25l");
    }

    NtClose(h);
}

static void fg() {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    bool tc_settings_valid = false;
    termios tc_settings;
    nt_status status;
    uintptr_t pgid;
    uint32_t retlen;

    // FIXME - job_spec

    if (!tcgetattr(STDOUT_FILENO, &tc_settings))
        tc_settings_valid = true;

    if (is_list_empty(suspended_processes)) {
        print_stderr("fg: current: no such job\n");
        return;
    }

    auto& sp = containing_record<suspended_process>(suspended_processes.prev);
    unique_handle proc;

    swap(sp.proc, proc);

    if (tcsetattr(STDOUT_FILENO, TCSANOW, &sp.tcsettings))
        print_stderr("tcsetattr failed (errno {})\n", errno_to_string_view(*_errno()));

    remove_entry_list(sp.le);
    delete &sp;

    num_suspended_processes--;

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxGetProcessGroup,
                                       &pgid, sizeof(pgid), &retlen);
    if (!success(status))
        print_stderr("NtQueryInformationProcess returned {} for MoxnoxGetProcessGroup\n", status);
    else
        set_pty_fg_proc(peb.ProcessParameters->StandardOutput, pgid);

    NtResumeProcess(proc.get());

    running_proc = proc.get();
    run_program(proc, tc_settings_valid ? &tc_settings : nullptr);
}

static void get_prompt() {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto cwd = peb->ProcessParameters->CurrentDirectory.Handle;

    // FIXME - parse PS1 environment variable
    // FIXME - prompt_buf not freed on exit

    static const string_view prefix = "\x01\e[01;34m\x02";
    static const string_view suffix = " $ \x01\e[0m\x02";

    status = NtQueryInformationFile(cwd, &iosb, &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        print_stderr("NtQueryInformationFile returned {}\n", status);
        return;
    }

    size_t fnilen = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(fnilen);
    if (!fni2) {
        print_stderr("malloc failed\n");
        return;
    }

    status = NtQueryInformationFile(cwd, &iosb, fni2, (uint32_t)fnilen,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        print_stderr("NtQueryInformationFile returned {}\n", status);
        free(fni2);
        return;
    }

    auto u16sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));
    auto pwdlen = utf16_to_utf8_len(u16sv);

    auto buf = (char*)malloc(prefix.size() + pwdlen + suffix.size() + 1);
    if (!buf) {
        print_stderr("malloc failed\n");
        free(fni2);
    }

    utf16_to_utf8(span(buf + prefix.size(), pwdlen), u16sv);

    memcpy(buf, prefix.data(), prefix.size());
    memcpy(&buf[prefix.size() + pwdlen], suffix.data(), suffix.size());
    buf[prefix.size() + pwdlen + suffix.size()] = 0;

    if (prompt_buf)
        free(prompt_buf);

    prompt_buf = buf;
    prompt = string_view(prompt_buf, prefix.size() + pwdlen + suffix.size());
}

static void process_line(string_view sv) {
    if (!sv.empty() && sv.back() == '\r')
        sv = string_view(sv.data(), sv.size() - 1);

    while (!sv.empty() && sv.front() == ' ') {
        sv = string_view(sv.data() + 1, sv.size() - 1);
    }

    if (sv.empty())
        return;

    auto cmd = sv;
    auto param = string_view();

    bool quotes = false;
    for (unsigned int i = 0; i < sv.size(); i++) {
        if (sv[i] == '"') // FIXME - escaped quotes
            quotes = !quotes;
        else if (sv[i] == ' ' && !quotes) {
            cmd = string_view(sv.data(), i);
            param = string_view(sv.data() + i + 1, sv.size() - i - 1);
            break;
        }
    }

    if (cmd.front() == '"' && cmd.back() == '"')
        cmd = string_view(cmd.data() + 1, cmd.size() - 2);

    if (cmd.empty())
        return;

    if (cmd == "time")
        time();
    else if (cmd == "timer")
        timer();
    else if (cmd == "segfault")
        segfault(); // TESTING
    else if (cmd == "inline")
        inline1(); // TESTING
    else if (cmd == "int3")
        int3(); // TESTING
    else if (cmd == "ill")
        illegal_instruction(); // TESTING
    else if (cmd == "div")
        divide_by_zero(); // TESTING
#ifdef __x86_64__
    else if (cmd == "int1")
        int1(); // TESTING
    else if (cmd == "gpf")
        general_protection_fault(); // TESTING
#endif
    else if (cmd == "scroll") // TESTING
        scroll(param);
    else if (cmd == "cursor") // TESTING
        cursor();
    else if (cmd == "fg")
        fg();
    else if (cmd == "cd") {
        cd(param);
        get_prompt();
    } else
        try_run_program(cmd, sv);
}

static const char* const builtins[] = {
    "exit",
    "time",
    "timer",
    "segfault",
    "inline",
    "int3",
    "ill",
    "div",
#ifdef __x86_64__
    "int1",
    "gpf",
#endif
    "scroll",
    "cursor",
    "fg",
    "cd",
    nullptr
};

static char* command_generator(const char* text, int state) {
    static unsigned int builtin_idx;
    static char* path_copy;
    static char* path_pos;
    static DIR* dir;
    static char dir_path[4096];

    size_t textlen = strlen(text);

    if (state == 0) {
        builtin_idx = 0;

        free(path_copy);
        path_copy = nullptr;
        path_pos = nullptr;

        if (dir) {
            closedir(dir);
            dir = nullptr;
        }

        auto path = getenv("PATH");

        if (path && path[0]) {
            path_copy = strdup(path);
            path_pos = path_copy;
        }
    }

    while (builtins[builtin_idx]) {
        auto name = builtins[builtin_idx];
        builtin_idx++;

        if (strncmp(name, text, textlen) == 0)
            return strdup(name);
    }

    while (true) {
        if (dir) {
            struct dirent* ent;

            while ((ent = readdir(dir)) != nullptr) {
                if (ent->d_type != DT_REG && ent->d_type != DT_UNKNOWN && ent->d_type != DT_LNK)
                    continue;

                string_view name(ent->d_name);

                if (name.size() > 4 && name.substr(name.size() - 4) == ".exe")
                    name = name.substr(0, name.size() - 4);

                if (name.size() < textlen || strncmp(name.data(), text, textlen) != 0)
                    continue;

                auto result = (char*)malloc(name.size() + 1);
                if (!result)
                    return nullptr;

                memcpy(result, name.data(), name.size());
                result[name.size()] = '\0';

                return result;
            }

            closedir(dir);
            dir = nullptr;
        }

        if (!path_pos || !*path_pos)
            break;

        auto col = strchr(path_pos, ':');
        size_t dirlen;

        if (col) {
            dirlen = (size_t)(col - path_pos);
            memcpy(dir_path, path_pos, dirlen);
            dir_path[dirlen] = '\0';
            path_pos = col + 1;
        } else {
            dirlen = strlen(path_pos);
            memcpy(dir_path, path_pos, dirlen);
            dir_path[dirlen] = '\0';
            path_pos += dirlen;
        }

        if (dirlen == 0 || dirlen >= sizeof(dir_path))
            continue;

        dir = opendir(dir_path);
    }

    free(path_copy);
    path_copy = nullptr;
    path_pos = nullptr;

    return nullptr;
}

static void get_pty_info_stream() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    nt_status status;
    IO_STATUS_BLOCK iosb;

    status = NtDeviceIoControlFile(peb->ProcessParameters->StandardOutput, 0,
                                   nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTY_GET_INFO_STREAM,
                                   nullptr, 0, &info_stream, sizeof(info_stream));

    if (!success(status))
        print_stderr("IOCTL_MOXNOX_PTY_GET_INFO_STREAM returned {}\n", status);
}

static void init_history() {
    struct passwd pwd, *pwdres;
    char pwdbuf[1024];

    static const string_view suffix = "/.shell_history";

    if (getpwuid_r(getuid(), &pwd, pwdbuf, sizeof(pwdbuf), &pwdres) != 0 || !pwdres) {
        print_stderr("getpwuid_r failed\n");
        return;
    }

    if (!pwdres->pw_dir[0])
        return;

    auto home = string_view(pwdres->pw_dir);

    history_file = (char*)malloc(home.size() + suffix.size() + 1);
    if (!history_file) {
        print_stderr("malloc failed\n");
        return;
    }

    memcpy(history_file, home.data(), home.size());
    memcpy(history_file + home.size(), suffix.data(), suffix.size());
    history_file[home.size() + suffix.size()] = 0;

    read_history(history_file);
}

static void sigint_handler(int) {
    if (running_proc == 0)
        siglongjmp(sigint_jmpbuf, 1);
}

static void sigterm_handler(int) {
    // if currently in readline, it will get interrupt and return NULL, and we will exit

    NtSetEvent(sigterm_event.get(), nullptr);
    received_sigterm = true;
}

int main(int argc, char** argv) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    HANDLE h;
    struct sigaction act;
    char* command = nullptr;
    bool added_to_history = false;

    static const option options[] = {
        { "", required_argument, nullptr, 'c' },
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "c:", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case 'c':
                command = strdup(optarg);
                if (!command) {
                    print_stderr("strdup failed\n");
                    return 1;
                }

                break;

            case '?':
                return 1;
        }
    }

    if (argc != optind) {
        print_stderr("shell: bad usage\n");

        if (command)
            free(command);

        return 1;
    }

    if (isatty(STDOUT_FILENO)) {
        get_pty_info_stream();
        set_pty_fg_proc(peb.ProcessParameters->StandardOutput, (uintptr_t)getpgid(0));
    }

    status = NtCreateEvent(&h, 0, nullptr, EVENT_TYPE::NotificationEvent, false);
    if (!success(status)) {
        print_stderr("NtCreateEvent returned {}\n", status);

        if (command)
            free(command);

        return 1;
    }

    sigterm_event.reset(h);

    if (command) {
        process_line(command);
        free(command);
        // FIXME - return value?
        return 0;
    }

    get_prompt();
    init_history();

    memset(&act, 0, sizeof(act));
    act.sa_handler = sigint_handler;

    if (sigaction(SIGINT, &act, nullptr) == -1) {
        print_stderr("sigaction failed (errno {})\n", *_errno());
        return 1;
    }

    act.sa_handler = sigterm_handler;

    if (sigaction(SIGTERM, &act, nullptr) == -1) {
        print_stderr("sigaction failed (errno {})\n", *_errno());
        return 1;
    }

    rl_catch_signals = 0;

    rl_attempted_completion_function = [](const char* text, int start, int /*end*/) -> char** {
        if (start == 0)
            return rl_completion_matches(text, command_generator);

        return nullptr;
    };

    while (true) {
        if (received_sigterm)
            break;

        if (sigsetjmp(sigint_jmpbuf, 1) == 1) {
            print("^C\n");

            rl_free_line_state();
            rl_cleanup_after_signal();
        }

        auto line = readline(prompt.data());

        if (!line)
            break;

        if (*line) {
            add_history(line);
            added_to_history = true;
        }

        if (!strcmp(line, "exit")) {
            free(line);
            break;
        }

        process_line(line);
        free(line);
    }

    if (history_file) {
        // FIXME - write_history can race with other shells running (use cross-process mutex?)

        if (added_to_history)
            write_history(history_file);

        free(history_file);
    }

    while (!is_list_empty(suspended_processes)) {
        auto& sp = containing_record<suspended_process>(suspended_processes.next);

        remove_entry_list(sp.le);

        auto status = NtTerminateProcess(sp.proc.get(), nt_status::CONTROL_C_EXIT);
        if (!success(status))
            print_stderr("NtTerminateProcess returned {}\n", status);

        delete &sp;
    }

    return 0;
}
