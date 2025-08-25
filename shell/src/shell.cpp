#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nt.h>
#include <pe.h>
#include <ntdll.h>
#include <ioctl.h>
#include <span>
#include <charconv>
#include <chrono>

using namespace std;

import shelllib;
import format;
import print;
import unicode;
import unique_handle;
import signal;

static char* buf = nullptr;
static unsigned int buflen = 0;
static unsigned int cursor_pos = 0;

static const size_t PAGE_SIZE = 4096;
static const uint64_t PAGE_SHIFT = 12;

static char escape_buffer[50];
static size_t escape_buffer_len = 0;
static uint8_t utf8_buf[5];
static unsigned int utf8_chars = 0;

static HANDLE running_proc = 0;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

static bool forward_stdin(HANDLE h, string_view s, HANDLE proc) {
    auto write = [](HANDLE h, string_view s) {
        nt_status status;
        IO_STATUS_BLOCK iosb;

        // FIXME - should loop until all sent
        status = NtWriteFile(h, 0, nullptr, nullptr, &iosb, s.data(),
                             (uint32_t)s.size(), nullptr, nullptr);
        if (!success(status))
            print("NtWriteFile returned {}\n", status);
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
            print("NtSuspendProcess returned {}\n", status);

        // FIXME - print "Stopped" message

        return false;
    } while (true);

    if (!s.empty())
        write(h, s);

    return true;
}

static void run_program(HANDLE server_handle, HANDLE proc) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    do {
        IO_STATUS_BLOCK iosb;
        array<HANDLE, 3> objs;

        objs[0] = server_handle;
        objs[1] = proc;
        objs[2] = peb->ProcessParameters->StandardInput;

        auto status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                               true, nullptr);

        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print("NtWaitForMultipleObjects returned {}\n", status);
            return;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: {
                char s[PAGE_SIZE];

                // print pipe data

                status = NtReadFile(server_handle, 0, nullptr, nullptr,
                                    &iosb, s, sizeof(s), nullptr, nullptr);
                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    continue;
                }

                status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                                     nullptr, nullptr, &iosb, s,
                                     (uint32_t)iosb.Information, nullptr, nullptr);
                if (!success(status))
                    print("NtWriteFile returned {}\n", status);

                break;
            }

            case 1: {
                // process finished

                running_proc = 0;

                break;
            }

            case 2: {
                char s[PAGE_SIZE];

                // forward stdin to process

                status = NtReadFile(peb->ProcessParameters->StandardInput, 0,
                                    nullptr, nullptr, &iosb, s, sizeof(s),
                                    nullptr, nullptr);
                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    continue;
                }

                if (iosb.Information == 0)
                    break;

                if (!forward_stdin(server_handle, string_view(s, iosb.Information),
                                   proc))
                    running_proc = 0;

                break;
            }
        }
    } while (running_proc != 0);
}

static void start_program(HANDLE sect, const SECTION_IMAGE_INFORMATION& sii,
                          string_view cmdline, HANDLE server_handle,
                          HANDLE client_handle) {
    NTSTATUS status;
    unique_handle proc, thread;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    HANDLE stdinh = 0, stdouth = 0, curdirh = 0;
    PROCESS_BASIC_INFORMATION pbi;
    PEB newpeb;
    RTL_USER_PROCESS_PARAMETERS params;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect, 0, 0);

    if (!success(status)) {
        print("NtCreateProcess returned {}\n", status);
        return;
    }

    proc.reset(h);

    // duplicate handles

    status = NtDuplicateObject(NtCurrentProcess(), client_handle,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);

    if (!success(status)) {
        print("NtDuplicateObject returned {}\n", status);
        return;
    }

    status = NtDuplicateObject(NtCurrentProcess(), client_handle,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);

    if (!success(status)) {
        print("NtDuplicateObject returned {}\n", status);
        return;
    }

    if (peb->ProcessParameters->CurrentDirectory.Handle) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle,
                                   proc.get(), &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            return;
        }
    }

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return;
    }

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    }

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    }

    // change handles

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stdouth;
    params.CurrentDirectory.Handle = curdirh;

    // copy current directory string

    params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                 peb->ProcessParameters->CurrentDirectory.DosPath.Length);

    status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer,
                                  peb->ProcessParameters->CurrentDirectory.DosPath.Buffer,
                                  params.CurrentDirectory.DosPath.Length, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        return;
    }

    // FIXME - ImagePathName
    // FIXME - environment variables

    // allocate misc block

    auto u16_cmdline_len = utf8_to_utf16_len(cmdline);

    size_t misc_block_size = (u16_cmdline_len + 1) * sizeof(char16_t);

    auto misc_block = (uint8_t*)malloc(misc_block_size);
    if (!misc_block) {
        print("malloc failed\n");
        return;
    }

    // allocate misc block in process

    void* proc_misc_block = nullptr;
    size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

    status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                     &proc_misc_block_size, MEM_COMMIT | MEM_RESERVE,
                                     PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        free(misc_block);
        return;
    }

    // copy cmdline

    utf8_to_utf16(span((char16_t*)misc_block, u16_cmdline_len), cmdline);
    misc_block[u16_cmdline_len * sizeof(char16_t)] = 0;
    params.CommandLine.Length = (uint16_t)(u16_cmdline_len * sizeof(char16_t));
    params.CommandLine.MaximumLength = (uint16_t)((u16_cmdline_len + 1) * sizeof(char16_t));
    params.CommandLine.Buffer = (char16_t*)proc_misc_block;

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        free(misc_block);
        return;
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params, sizeof(params), nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        return;
    }

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000;

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        return;
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
    context.Rip = (uintptr_t)sii.TransferAddress;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x20;
    context.EFlags = 0x200;
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc.get(),
                            &client_id, &context, &initial_teb, true);

    if (!success(status)) {
        print("NtCreateThread returned {}\n", status);
        return;
    }

    thread.reset(h);

    running_proc = proc.get();

    NtResumeThread(thread.get(), nullptr);

    run_program(server_handle, proc.get());
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
        print("NtQuerySystemTime returned {}\n", status);
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
        print("NtCreateTimer returned {}\n", status); // FIXME - should be to stderr
        return;
    }

    due_time.QuadPart = -10000000; // 1 second

    status = NtSetTimer(h, &due_time, nullptr, nullptr, false,
                        1000, nullptr);
    if (!success(status)) {
        print("NtSetTimer returned {}\n", status); // FIXME - should be to stderr
        NtClose(h);
        return;
    }

    for (unsigned int i = 0; i < 3; i++) {
        status = NtWaitForSingleObject(h, false, nullptr);
        if (!success(status)) {
            print("NtWaitForSingleObject returned {}\n", status); // FIXME - should be to stderr
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

static void halt() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownPowerOff);

    // shouldn't return

    print("NtShutdownSystem returned {}\n", status);
}

static void reboot() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownReboot);

    // shouldn't return

    print("NtShutdownSystem returned {}\n", status);
}

static void try_run_program(string_view program, string_view cmdline,
                            HANDLE server_handle, HANDLE client_handle) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h;
    unique_handle file, sect;
    IO_STATUS_BLOCK iosb;
    SECTION_IMAGE_INFORMATION sii;

    // FIXME - should be checking PATH instead of root

    auto len = utf8_to_utf16_len(program) + 1;

    // open file

    // FIXME - path may be absolute, or relative to subdir
    // FIXME - translate slashes to backslashes
    us.Length = us.MaximumLength = (uint16_t)(len * sizeof(char16_t));
    us.Buffer = (char16_t*)malloc(us.Length);

    if (!us.Buffer) {
        print("malloc failed\n");
        return;
    }

    us.Buffer[0] = '\\';
    utf8_to_utf16(span(us.Buffer + 1, len - 1), program);

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_NON_DIRECTORY_FILE, nullptr, 0);
    free(us.Buffer);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        bool add_ext = true;

        for (auto c : program) {
            if (c == '.' || c == '/' || c == '\\') {
                add_ext = false;
                break;
            }
        }

        if (add_ext) {
            static const char16_t suffix[] = u".exe";

            us.Length = us.MaximumLength = (uint16_t)((len * sizeof(char16_t)) + sizeof(suffix) - sizeof(char16_t));
            us.Buffer = (char16_t*)malloc(us.Length);

            if (!us.Buffer) {
                print("malloc failed\n");
                return;
            }

            // FIXME - PATH etc.
            us.Buffer[0] = '\\';
            utf8_to_utf16(span(us.Buffer + 1, len - 1), program);
            memcpy(us.Buffer + len, suffix, sizeof(suffix) - sizeof(char16_t));

            oa.ObjectName = &us;

            status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                                  0, 0, FILE_OPEN, 0, nullptr, 0);
        }
    }

    if (status == nt_status::OBJECT_NAME_NOT_FOUND || status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print("{}: command not found\n", program); // FIXME - stderr
        return;
    } else if (status == nt_status::FILE_IS_A_DIRECTORY) {
        print("{}: is a directory\n", program); // FIXME - stderr
        return;
    } else if (!success(status)) {
        print("NtCreateFile returned {}\n", status); // FIXME - stderr
        return;
    }

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());

    if (status == nt_status::INVALID_IMAGE_FORMAT) {
        print("{}: not an executable\n", program); // FIXME - stderr
        return;
    } else if (!success(status)) {
        print("NtCreateSection returned {}\n", status);
        return;
    }

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation, &sii,
                            sizeof(SECTION_IMAGE_INFORMATION), nullptr);

    if (!success(status)) {
        print("NtQuerySection returned {}\n", status); // FIXME - stderr
        return;
    }

    if (sii.ImageCharacteristics & IMAGE_FILE_DLL) {
        print("{}: not an executable\n", program); // FIXME - stderr
        return;
    }

    if (sii.Machine != IMAGE_FILE_MACHINE_AMD64) {
        print("{}: unsupported architecture\n", program); // FIXME - stderr
        return;
    }

    start_program(sect.get(), sii, cmdline, server_handle, client_handle);
}

static void int3() {
    __asm("int 3");
}

static void int1() {
    __asm __volatile(
        "pushf\n"
        "\tor QWORD PTR [rsp], 0x100\n"
        "\tpopf"
    );
}

static void illegal_instruction() {
    __asm("ud2");
}

static void divide_by_zero() {
    __asm __volatile(
        "xor rax, rax\n"
        "idiv rax\n"
    );
}

static void general_protection_fault() {
    *(uint8_t*)0x123456789abcdef0 = 1;
}

static void scroll(string_view param) {
    int val;

    if (param.empty()) {
        print("usage: scroll <lines>\n");
        return;
    }

    auto [ptr, ec] = from_chars(param.data(), param.data() + param.size(), val);

    if (ptr != param.data() + param.size()) {
        print("unable to parse \"{}\" as integer\n", param);
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
        print("NtCreateTimer returned {}\n", status); // FIXME - should be to stderr
        return;
    }

    print("\e[?25h");

    due_time.QuadPart = -10000000; // 1 second

    status = NtSetTimer(h, &due_time, nullptr, nullptr, false,
                        1000, nullptr);
    if (!success(status)) {
        print("NtSetTimer returned {}\n", status); // FIXME - should be to stderr
        NtClose(h);
        return;
    }

    for (unsigned int i = 0; i < 4; i++) {
        status = NtWaitForSingleObject(h, false, nullptr);
        if (!success(status)) {
            print("NtWaitForSingleObject returned {}\n", status); // FIXME - should be to stderr
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

static void process_line(string_view sv, HANDLE server_handle,
                         HANDLE client_handle) {
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

    if (cmd == "exit")
        NtTerminateProcess(NtCurrentProcess(), nt_status::SUCCESS);
    else if (cmd == "time")
        time();
    else if (cmd == "timer")
        timer();
    else if (cmd == "segfault")
        segfault(); // TESTING
    else if (cmd == "int3")
        int3(); // TESTING
    else if (cmd == "int1")
        int1(); // TESTING
    else if (cmd == "ill")
        illegal_instruction(); // TESTING
    else if (cmd == "div")
        divide_by_zero(); // TESTING
    else if (cmd == "gpf")
        general_protection_fault(); // TESTING
    else if (cmd == "scroll") // TESTING
        scroll(param);
    else if (cmd == "cursor") // TESTING
        cursor();
    else if (cmd == "halt")
        halt();
    else if (cmd == "reboot")
        reboot();
    else
        try_run_program(cmd, sv, server_handle, client_handle);
}

static void process_lines(HANDLE server_handle, HANDLE client_handle) {
    for (unsigned int i = cursor_pos; i < buflen; i++) {
        print("\x1b[C"); // right
    }
    print("\n");

    process_line(string_view(buf, buflen), server_handle, client_handle);

    free(buf);
    buf = nullptr;

    buflen = 0;

    cursor_pos = 0;
}

#if 0
static void thread_func() {
    print("Hello from second thread\n");

    // FIXME

    while (1) { }
}

static void thread_test() {
    NTSTATUS status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000;

    status = NtAllocateVirtualMemory(NtCurrentProcess(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned "); // FIXME - should be to stderr
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
    context.Rip = (uintptr_t)thread_func;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x20;
    context.EFlags = 0x200;
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, NtCurrentProcess(), &client_id, &context,
                            &initial_teb, true);

    if (!success(status)) {
        print("NtCreateThread returned "); // FIXME - should be to stderr
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    print("UniqueProcess = ");
    print_hex((uintptr_t)client_id.UniqueProcess);
    print(", UniqueThread = ");
    print_hex((uintptr_t)client_id.UniqueThread);
    print("\n");

    NtResumeThread(h, nullptr);

    NtClose(h);
}
#endif

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

static void handle_input(string_view s, HANDLE server_handle,
                         HANDLE client_handle) {
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
            print("> ");

            if (buf) {
                free(buf);
                buf = nullptr;
            }

            buflen = 0;
            cursor_pos = 0;
        } else if (c == '\x04') { // Ctrl+D
            if (buflen == 0) {
                print("\n");
                NtTerminateProcess(NtCurrentProcess(), nt_status::SUCCESS);
            }
        } else if (c == '\n') {
            process_lines(server_handle, client_handle);
            print("> ");
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

void do_print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

static void handle_ctrl_c() {
    if (running_proc != 0) {
        EXCEPTION_RECORD er;

        er.ExceptionFlags = 0;
        er.ExceptionRecord = nullptr;
        er.ExceptionCode = (uint32_t)nt_status::CONTROL_C_EXIT;
        er.NumberParameters = 0;

        send_signal_to_process(running_proc, er);
    } else
        handle_input("\x03", 0, 0);
}

static nt_status create_pipe(unique_handle& server_handle,
                             unique_handle& client_handle) {
    nt_status status;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    OBJECT_ATTRIBUTES oa;
    PROCESS_BASIC_INFORMATION pbi;
    char16_t pipename[50];
    HANDLE h;

    status = NtQueryInformationProcess(NtCurrentProcess(),
                                       PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return status;
    }

    {
        char s[50];
        span sp(s, sizeof(s));

        sp = format(sp, "\\dev\\shellpipe-{}", pbi.UniqueProcessId);

        auto name = string_view(s, (size_t)(sp.data() - s));

        utf8_to_utf16(pipename, name);

        us.Length = us.MaximumLength = (uint16_t)(utf8_to_utf16_len(name) * sizeof(char16_t));
        us.Buffer = pipename;
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateNamedPipeFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa,
                                   &iosb, 0, FILE_CREATE, 0, 1, 0, 0, 0, 0, 0,
                                   nullptr);
    if (!success(status)) {
        print("NtCreateNamedPipeFile returned {}\n", status);
        return status;
    }

    server_handle.reset(h);

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return status;
    }

    client_handle.reset(h);

    return nt_status::SUCCESS;
}

extern "C"
enum EXCEPTION_DISPOSITION exception_handler(EXCEPTION_RECORD* rec, void* /*frame*/,
                                             CONTEXT* /*context*/, DISPATCHER_CONTEXT* /*dispatch*/) {
    if ((enum nt_status)rec->ExceptionCode != nt_status::CONTROL_C_EXIT)
        return EXCEPTION_DISPOSITION::ExceptionContinueSearch;

    handle_ctrl_c();

    return EXCEPTION_DISPOSITION::ExceptionContinueExecution;
}

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
    __asm __volatile(".seh_handler exception_handler, @except");
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    unique_handle server_handle, client_handle;
    nt_status status;

//     thread_test();

    status = create_pipe(server_handle, client_handle);
    if (!success(status)) {
        NtTerminateThread(NtCurrentThread(), status);
        return;
    }

    print("> ");

    while (true) {
        IO_STATUS_BLOCK iosb;
        char s[10];

        status = NtReadFile(peb->ProcessParameters->StandardInput, 0, nullptr, nullptr, &iosb,
                            s, sizeof(s), nullptr, nullptr);

        if (!success(status)) {
            print("NtReadFile returned {}\n", status); // FIXME - should be to stderr
            break;
        }

        if (iosb.Information == 0)
            continue;

        auto buf2 = (char*)malloc(buflen + iosb.Information);
        if (!buf2) {
            print("out of memory\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        }

        if (buf) {
            memcpy(buf2, buf, buflen);
            free(buf);
        }

        buf = buf2;

        handle_input(string_view(s, iosb.Information), server_handle.get(),
                     client_handle.get());
    }

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
