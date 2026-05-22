#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <winerror.h>
#include <span>
#include <charconv>
#include <chrono>
#include <utility>

using namespace std;

import nt;
import ntdll;
import format;
import print;
import unicode;
import unique_handle;
import signal;
import list;
import pe;
import kernelbase;
import pagesize;
import cd;
import termios;
import moxnox_ioctls;

static char* buf = nullptr;
static unsigned int buflen = 0;
static unsigned int cursor_pos = 0;

static char escape_buffer[50];
static size_t escape_buffer_len = 0;
static uint8_t utf8_buf[5];
static unsigned int utf8_chars = 0;

static HANDLE running_proc = 0;
static HANDLE ctrl_c_event = 0;

static char* prompt_buf;
static string_view prompt;

#if 0
struct suspended_process {
    list_entry le;
    unique_handle proc;
};

static list_entry suspended_processes = { &suspended_processes, &suspended_processes };
static unsigned int num_suspended_processes = 0;
#endif

void* operator new(size_t count) {
    return malloc(count);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

#if 0
static bool forward_stdin(HANDLE h, string_view s, HANDLE proc) {
    auto write = [](HANDLE h, string_view s) {
        nt_status status;
        IO_STATUS_BLOCK iosb;

        // FIXME - should loop until all sent
        status = NtWriteFile(h, 0, nullptr, nullptr, &iosb, s.data(),
                             (uint32_t)s.size(), nullptr, nullptr);
        if (!success(status))
            print_stderr("NtWriteFile returned {}\n", status);
    };

    size_t ctrlz;

    do {
        ctrlz = s.find_first_of('\x1a');
        if (ctrlz == string_view::npos)
            break;

        if (ctrlz != 0)
            write(h, string_view(s.data(), ctrlz));

        auto status = NtSuspendProcess(proc);
        if (!success(status))
            print_stderr("NtSuspendProcess returned {}\n", status);

        // FIXME - bash also prints command line
        print("[{}]+  Stopped\n", num_suspended_processes + 1);

        return false;
    } while (true);

    if (!s.empty())
        write(h, s);

    return true;
}
#endif

static void run_program(unique_handle& proc, const termios* tc_settings) {
    nt_status status;

    do {
        status = NtWaitForSingleObject(proc.get(), true, nullptr);

        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print_stderr("NtWaitForSingleObject returned {}\n", status);
            return;
        }

        running_proc = 0;

        break;
    } while (running_proc != 0);

    if (tc_settings)
        tcsetattr(STDOUT_FILENO, TCSANOW, tc_settings);

    PROCESS_BASIC_INFORMATION pbi;

    status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        print_stderr("NtQueryInformationProcess returned {}\n", status);
    else if (pbi.ExitStatus == nt_status::CONTROL_C_EXIT)
        print("^C\n");

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

static bool set_pty_fg_proc(HANDLE client_handle, HANDLE proc) {
    nt_status status;
    IO_STATUS_BLOCK iosb;

    status = NtDeviceIoControlFile(client_handle, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS,
                                   &proc, sizeof(proc), nullptr, 0);
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS returned {}\n", status);
        return false;
    }

    return true;
}

static void try_run_program(string_view program, string_view cmdline) {
    termios tc_settings;
    bool tc_settings_valid;
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    auto u16cmdlinelen = utf8_to_utf16_len(cmdline);
    auto u16cmdline = (char16_t*)malloc((u16cmdlinelen + 1) * sizeof(char16_t));
    if (!u16cmdline) {
        print_stderr("malloc failed\n");
        return;
    }

    utf8_to_utf16(span(u16cmdline, u16cmdlinelen), cmdline);
    u16cmdline[u16cmdlinelen] = 0;

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

    set_pty_fg_proc(peb.ProcessParameters->StandardOutput, proc.get());

    auto status = NtResumeThread(pi.hThread, nullptr);
    if (!success(status)) {
        print_stderr("NtResumeThread returned {}\n", status);
        running_proc = 0;
        NtClose(pi.hThread);
        set_pty_fg_proc(peb.ProcessParameters->StandardOutput,
                        NtCurrentProcess());
        return;
    }

    NtClose(pi.hThread);

    run_program(proc, tc_settings_valid ? &tc_settings : nullptr);

    set_pty_fg_proc(peb.ProcessParameters->StandardOutput,
                    NtCurrentProcess());
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

static void exit() {
#if 0
    while (!is_list_empty(suspended_processes)) {
        auto& sp = containing_record<suspended_process>(suspended_processes.next);

        remove_entry_list(sp.le);

        auto status = NtTerminateProcess(sp.proc.get(), nt_status::CONTROL_C_EXIT);
        if (!success(status))
            print_stderr("NtTerminateProcess returned {}\n", status);

        delete &sp;
    }
#endif

    NtTerminateProcess(NtCurrentProcess(), nt_status::SUCCESS);
}

#if 0
static void fg(HANDLE server_handle) {
    // FIXME - job_spec

    if (is_list_empty(suspended_processes)) {
        print_stderr("fg: current: no such job\n");
        return;
    }

    auto& sp = containing_record<suspended_process>(suspended_processes.prev);
    unique_handle proc;

    swap(sp.proc, proc);

    remove_entry_list(sp.le);
    delete &sp;

    num_suspended_processes--;

    NtResumeProcess(proc.get());

    running_proc = proc.get();
    run_program(server_handle, proc);
}
#endif

static void get_prompt() {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto cwd = peb->ProcessParameters->CurrentDirectory.Handle;

    // FIXME - parse PS1 environment variable
    // FIXME - prompt_buf not freed on exit

    static const string_view prefix = "\e[01;34m";
    static const string_view suffix = " $ \e[0m";

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

    auto buf = (char*)malloc(prefix.size() + pwdlen + suffix.size());
    if (!buf) {
        print_stderr("malloc failed\n");
        free(fni2);
    }

    utf16_to_utf8(span(buf + prefix.size(), pwdlen), u16sv);

    memcpy(buf, prefix.data(), prefix.size());
    memcpy(&buf[prefix.size() + pwdlen], suffix.data(), suffix.size());

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

    if (cmd == "exit")
        exit();
    else if (cmd == "time")
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
#if 0
    else if (cmd == "fg")
        fg(server_handle);
#endif
    else if (cmd == "cd") {
        cd(param);
        get_prompt();
    } else
        try_run_program(cmd, sv);
}

static void process_lines() {
    for (unsigned int i = cursor_pos; i < buflen; i++) {
        print("\x1b[C"); // right
    }
    print("\n");

    process_line(string_view(buf, buflen));

    free(buf);
    buf = nullptr;

    buflen = 0;

    cursor_pos = 0;
}

static void handle_delete() {
    unsigned int cp_size;

    if (cursor_pos == buflen)
        return;

    if (!(buf[cursor_pos] & 0x80))
        cp_size = 1;
    else if ((buf[cursor_pos] & 0xe0) == 0xc0)
        cp_size = 2;
    else if ((buf[cursor_pos] & 0xf0) == 0xe0)
        cp_size = 3;
    else if ((buf[cursor_pos] & 0xf8) == 0xf0)
        cp_size = 4;
    else
        return;

    if (cp_size == 0)
        return;

    memmove(&buf[cursor_pos], &buf[cursor_pos + cp_size],
            buflen - cursor_pos - cp_size);

    buflen -= cp_size;

    print("\x1b[K"); // erase rest of line

    auto sv = string_view(&buf[cursor_pos], buflen - cursor_pos);

    print("{}", sv);

    for (auto c : sv) {
        // skip if not start of code point
        if ((c & 0xc0) == 0x80)
            continue;

        print("\x1b[D"); // left
    }
}

static void handle_escape_sequence(string_view s) {
    if (s == "\x1b[D") { // left
        if (cursor_pos == 0)
            return;

        // find start of UTF-8 sequence
        while ((buf[cursor_pos - 1] & 0xc0) == 0x80 && cursor_pos >= 2) {
            cursor_pos--;
        }

        cursor_pos--;
        print("\x1b[D");
    } else if (s == "\x1b[C") { // right
        if (cursor_pos == buflen)
            return;

        if (!(buf[cursor_pos] & 0x80))
            cursor_pos++;
        else if ((buf[cursor_pos] & 0xe0) == 0xc0 && cursor_pos < buflen - 1)
            cursor_pos += 2;
        else if ((buf[cursor_pos] & 0xf0) == 0xe0 && cursor_pos < buflen - 2)
            cursor_pos += 3;
        else if ((buf[cursor_pos] & 0xf8) == 0xf0 && cursor_pos < buflen - 3)
            cursor_pos += 4;

        print("\x1b[C");
    } else if (s == "\x1b[H") { // home
        if (cursor_pos == 0)
            return;

        for (size_t i = 0; i < cursor_pos; i++) {
            if ((buf[i] & 0xc0) == 0x80)
                continue;

            print("\x1b[D"); // left
        }

        cursor_pos = 0;
    } else if (s == "\x1b[F") { // end
        for (size_t i = cursor_pos; i < buflen; i++) {
            if ((buf[i] & 0xc0) == 0x80)
                continue;

            print("\x1b[C"); // right
        }

        cursor_pos = buflen;
    } else if (s == "\x1b[3~") // delete
        handle_delete();
}

static void handle_char2(char32_t c) {
    if (c < 0x20)
        return;

    unsigned int cp_len = 1;

    if (c < 0x80)
        cp_len = 1;
    else if (c < 0x800)
        cp_len = 2;
    else if (c < 0x10000)
        cp_len = 3;
    else if (c < 0x110000)
        cp_len = 4;
    else
        return;

    memmove(&buf[cursor_pos + cp_len], &buf[cursor_pos], buflen - cursor_pos);

    if (c < 0x80)
        buf[cursor_pos] = (char)c;
    else if (c < 0x800) {
        buf[cursor_pos] = (char)(0xc0 | ((c & 0x7c0) >> 6));
        buf[cursor_pos + 1] = (char)(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        buf[cursor_pos] = (char)(0xe0 | ((c & 0xf000) >> 12));
        buf[cursor_pos + 1] = (char)(0x80 | ((c & 0xfc0) >> 6));
        buf[cursor_pos + 2] = (char)(0x80 | (c & 0x3f));
    } else if (c < 0x110000) {
        buf[cursor_pos] = (char)(0xf0 | ((c & 0xe0000) >> 18));
        buf[cursor_pos + 1] = (char)(0x80 | ((c & 0x3f000) >> 12));
        buf[cursor_pos + 2] = (char)(0x80 | ((c & 0xfc0) >> 6));
        buf[cursor_pos + 3] = (char)(0x80 | (c & 0x3f));
    }

    buflen += cp_len;

    print("\x1b[K"); // erase rest of line
    print("{}", string_view(buf + cursor_pos, buflen - cursor_pos));

    cursor_pos += cp_len;

    for (unsigned int i = cursor_pos; i < buflen; i++) {
        // skip if not start of code point
        if ((buf[i] & 0xc0) == 0x80)
            continue;

        print("\x1b[D");
    }
}

static void handle_char(char c) {
    if (utf8_chars != 0) {
        if (!(c & 0x80) || utf8_chars == sizeof(utf8_buf)) {
            handle_char2(0xfffd);
            handle_char2((uint8_t)c);
            utf8_chars = 0;
            return;
        }

        utf8_buf[utf8_chars] = (uint8_t)c;
        utf8_chars++;

        if ((utf8_buf[0] & 0xe0) == 0xc0 && utf8_chars == 2) {
            if ((utf8_buf[1] & 0xc0) != 0x80) {
                handle_char2(0xfffd);
                utf8_chars = 0;
                return;
            }

            auto c = (char32_t)((utf8_buf[0] & 0x1f) << 6) | (char32_t)(utf8_buf[1] & 0x3f);

            handle_char2(c);
            utf8_chars = 0;
            return;
        } else if ((utf8_buf[0] & 0xf0) == 0xe0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    handle_char2(0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    handle_char2(0xfffd);
                    utf8_chars = 0;
                    return;
                }

                auto c = (char32_t)((utf8_buf[0] & 0xf) << 12) | (char32_t)((utf8_buf[1] & 0x3f) << 6) | (char32_t)(utf8_buf[2] & 0x3f);

                handle_char2(c);
                utf8_chars = 0;
            }

            return;
        } else if ((utf8_buf[0] & 0xf8) == 0xf0) {
            if (utf8_chars == 2) {
                if ((utf8_buf[1] & 0xc0) != 0x80) {
                    handle_char2(0xfffd);
                    utf8_chars = 0;
                }
            } else if (utf8_chars == 3) {
                if ((utf8_buf[2] & 0xc0) != 0x80) {
                    handle_char2(0xfffd);
                    utf8_chars = 0;
                }
            } else {
                if ((utf8_buf[3] & 0xc0) != 0x80) {
                    handle_char2(0xfffd);
                    utf8_chars = 0;
                    return;
                }

                auto c = (char32_t)((utf8_buf[0] & 0x7) << 18) | (char32_t)((utf8_buf[1] & 0x3f) << 12) | (char32_t)((utf8_buf[2] & 0x3f) << 6) | (char32_t)(utf8_buf[3] & 0x3f);

                handle_char2(c);
                utf8_chars = 0;
            }

            return;
        }
    }

    if ((c & 0xc0) == 0xc0) { // first character of UTF-8 sequence
        utf8_buf[0] = (uint8_t)c;
        utf8_chars = 1;
        return;
    } else if (c & 0x80) {
        handle_char2(0xfffd);
        return;
    }

    handle_char2((char32_t)c);
}

static void handle_backspace() {
    unsigned int cp_size = 0;

    if (cursor_pos == 0)
        return;

    if (cursor_pos >= 1 && !(buf[cursor_pos - 1] & 0x80))
        cp_size = 1;
    else if (cursor_pos >= 2 && (buf[cursor_pos - 1] & 0xc0) == 0x80) {
        if ((buf[cursor_pos - 2] & 0xe0) == 0xc0)
            cp_size = 2;
        else if (cursor_pos >= 3 && (buf[cursor_pos - 2] & 0xc0) == 0x80) {
            if ((buf[cursor_pos - 3] & 0xf0) == 0xe0)
                cp_size = 3;
            else if (cursor_pos >= 4 && (buf[cursor_pos - 3] & 0xc0) == 0x80 && (buf[cursor_pos - 4] & 0xf8) == 0xf0)
                cp_size = 4;
        }
    }

    if (cp_size == 0)
        return;

    memmove(&buf[cursor_pos - cp_size], &buf[cursor_pos], buflen - cursor_pos);

    buflen -= cp_size;
    cursor_pos -= cp_size;

    print("\b");
    print("\x1b[K"); // erase rest of line

    auto sv = string_view(&buf[cursor_pos], buflen - cursor_pos);

    print("{}", sv);

    for (auto c : sv) {
        // skip if not start of code point
        if ((c & 0xc0) == 0x80)
            continue;

        print("\x1b[D"); // left
    }
}

static void handle_input(string_view s) {
    if (escape_buffer_len != 0) {
        if (escape_buffer_len == 1 && (s[0] != '[' && s[0] != 'O')) {
            handle_char(0x1b);
            escape_buffer_len = 0;
        } else {
            if (escape_buffer_len == 1) {
                escape_buffer[1] = s[0];
                escape_buffer_len++;

                s = string_view(s.data() + 1, s.size() - 1);

                if (s.empty())
                    return;
            }

            if (escape_buffer[1] == '[') {
                while (!s.empty() && s.front() >= 0x30 && s.front() <= 0x3f) {
                    if (escape_buffer_len == sizeof(escape_buffer)) {
                        for (size_t i = 0; i < escape_buffer_len; i++) {
                            handle_char(escape_buffer[i]);
                        }

                        escape_buffer_len = 0;
                        goto loop;
                    }

                    escape_buffer[escape_buffer_len] = s.front();
                    escape_buffer_len++;
                    s = string_view(s.data() + 1, s.size() - 1);
                }

                if (s.empty())
                    return;

                if (s.front() >= 0x40 && s.front() <= 0x7e) {
                    escape_buffer[escape_buffer_len] = s.front();
                    escape_buffer_len++;

                    s = string_view(s.data() + 1, s.size() - 1);

                    handle_escape_sequence(string_view(escape_buffer, escape_buffer_len));
                    escape_buffer_len = 0;

                    if (s.empty())
                        return;
                } else {
                    for (size_t i = 0; i < escape_buffer_len; i++) {
                        handle_char(escape_buffer[i]);
                    }

                    escape_buffer_len = 0;
                }
            } else if (escape_buffer[1] == 'O') {
                if (s.front() >= 'A' && s.front() <= 'Z') {
                    escape_buffer[escape_buffer_len] = s.front();
                    escape_buffer_len++;
                    s = string_view(s.data() + 1, s.size() - 1);

                    handle_escape_sequence(string_view(escape_buffer, escape_buffer_len));
                } else {
                    for (size_t i = 0; i < escape_buffer_len; i++) {
                        handle_char(escape_buffer[i]);
                    }
                }

                escape_buffer_len = 0;

                if (s.empty())
                    return;

                goto loop;
            }
        }
    }

loop:
    for (; !s.empty(); s = string_view(s.data() + 1, s.size() - 1)) {
        auto c = s.front();

        if (c == '\b')
            handle_backspace();
        else if (c == '\x03') { // Ctrl+C
            print("^C\n");
            print("{}", prompt);

            if (buf) {
                free(buf);
                buf = nullptr;
            }

            buflen = 0;
            cursor_pos = 0;
        } else if (c == '\n') {
            process_lines();
            print("{}", prompt);
        } else if (c == 0x1b) {
            if (s.size() == 1) {
                escape_buffer[0] = 0x1b;
                escape_buffer_len = 1;
                break;
            }

            if (s[1] == '[') {
                unsigned int param_bytes = 0;

                while (s.size() >= param_bytes + 2) {
                    if (s[2 + param_bytes] >= 0x30 && s[2 + param_bytes] <= 0x3f)
                        param_bytes++;
                    else
                        break;
                }

                if (s.size() == param_bytes + 2) {
                    if (param_bytes + 2 > sizeof(escape_buffer)) {
                        for (size_t i = 0; i < escape_buffer_len; i++) {
                            handle_char(escape_buffer[i]);
                        }
                        escape_buffer_len = 0;

                        for (size_t i = 0; i < s.size(); i++) {
                            handle_char(s[i]);
                        }

                        break;
                    }

                    memcpy(escape_buffer, s.data(), param_bytes + 2);
                    escape_buffer_len = param_bytes + 2;
                    break;
                }

                if (s[param_bytes + 2] >= 0x40 && s[param_bytes + 2] <= 0x7e) {
                    // cap mid-buffer escape sequences as sizeof(escape_buffer), for consistency
                    if (param_bytes + 3 > sizeof(escape_buffer)) {
                        for (size_t i = 0; i < param_bytes + 3; i++) {
                            handle_char(s[i]);
                        }
                    } else
                        handle_escape_sequence(string_view(s.data(), param_bytes + 3));

                    s = string_view(s.data() + param_bytes + 2, s.size() - param_bytes - 2);
                    continue;
                }
            } else if (s[1] == 'O') {
                if (s.size() == 2) {
                    escape_buffer[0] = 0x1b;
                    escape_buffer[1] = s[1];
                    escape_buffer_len = 2;
                    return;
                }

                if (s[2] >= 'A' && s[2] <= 'Z')
                    handle_escape_sequence(string_view(s.data(), 3));
                else {
                    for (size_t i = 0; i < 3; i++) {
                        handle_char(s[i]);
                    }
                }

                s = string_view(s.data() + 2, s.size() - 2);
                continue;
            }
        } else
            handle_char(c);
    }
}

static bool ctrl_handler(uint32_t type) {
    if (type != CTRL_C_EVENT)
        return false;

    auto status = NtSetEvent(ctrl_c_event, nullptr);
    if (!success(status)) {
        print_stderr("NtSetEvent returned {}\n", status);
        return false;
    }

    return true;
}

int main() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    nt_status status;

    status = NtCreateEvent(&ctrl_c_event, 0, nullptr,
                           EVENT_TYPE::SynchronizationEvent, false);
    if (!success(status)) {
        print_stderr("NtCreateEvent returned {}\n", status);
        return 1;
    }

    if (!SetConsoleCtrlHandler(ctrl_handler, true))
        print_stderr("SetConsoleCtrlHandler failed\n");

    get_prompt();
    print("{}", prompt);

    while (true) {
        array<HANDLE, 2> objs;
        IO_STATUS_BLOCK iosb;
        char s[10];

        objs[0] = peb->ProcessParameters->StandardInput;
        objs[1] = ctrl_c_event;

        status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                          false, nullptr);
        if (!success(status)) {
            print_stderr("NtWaitForMultipleObjects returned {}\n", status);
            break;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: { // stdin
                status = NtReadFile(peb->ProcessParameters->StandardInput, 0,
                                    nullptr, nullptr, &iosb, s, sizeof(s),
                                    nullptr, nullptr);

                if (status == nt_status::END_OF_FILE) { // Ctrl+D
                    if (buflen == 0) {
                        print("\n");
                        NtTerminateProcess(NtCurrentProcess(), nt_status::SUCCESS);
                        unreachable();
                    }

                    continue;
                } else if (!success(status)) {
                    print_stderr("NtReadFile returned {}\n", status);
                    break;
                }

                if (iosb.Information == 0)
                    continue;

                buf = (char*)realloc(buf, buflen + iosb.Information);
                if (!buf) {
                    print_stderr("out of memory\n");
                    return 1;
                }

                handle_input(string_view(s, iosb.Information));

                break;
            }

            case 1: // Ctrl+C
                handle_input("\x03");
                break;
        }
    }

    return 0;
}
