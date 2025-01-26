#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nt.h>
#include <ntdll.h>
#include <ioctl.h>
#include <span>
#include <charconv>
#include <chrono>
#include "shell.h"

using namespace std;

char* buf = nullptr;
unsigned int buflen = 0;

static const size_t PAGE_SIZE = 4096;
static const uint64_t PAGE_SHIFT = 12;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static void print(integral auto i) {
    char s[21];

    auto [ptr, ec] = to_chars(s, s + sizeof(s), i);
    print(string_view(s, ptr - s));
}

static void print(const chrono::year_month_day& ymd) {
    char s[13];
    char* ptr = s;

    auto tcr = to_chars(ptr, s + sizeof(s), (int)ymd.year());
    ptr = tcr.ptr;
    *ptr = '-';
    ptr++;

    if ((unsigned int)ymd.month() < 10) {
        *ptr = '0';
        ptr++;
    }

    tcr = to_chars(ptr, s + sizeof(s), (unsigned int)ymd.month());
    ptr = tcr.ptr;
    *ptr = '-';
    ptr++;

    if ((unsigned int)ymd.day() < 10) {
        *ptr = '0';
        ptr++;
    }

    tcr = to_chars(ptr, s + sizeof(s), (unsigned int)ymd.day());
    ptr = tcr.ptr;

    print(string_view(s, ptr));
}

static void print(const chrono::hh_mm_ss<chrono::seconds>& hms) {
    char s[9];
    char* ptr = s;

    if (hms.hours().count() < 10) {
        *ptr = '0';
        ptr++;
    }

    auto tcr = to_chars(ptr, s + sizeof(s), hms.hours().count());
    ptr = tcr.ptr;
    *ptr = ':';
    ptr++;

    if (hms.minutes().count() < 10) {
        *ptr = '0';
        ptr++;
    }

    tcr = to_chars(ptr, s + sizeof(s), hms.minutes().count());
    ptr = tcr.ptr;
    *ptr = ':';
    ptr++;

    if (hms.seconds().count() < 10) {
        *ptr = '0';
        ptr++;
    }

    tcr = to_chars(ptr, s + sizeof(s), hms.seconds().count());
    ptr = tcr.ptr;

    print(string_view(s, ptr));
}

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

static void run_program(HANDLE file, string_view cmdline) {
    NTSTATUS status;
    HANDLE sect, proc, thread;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    HANDLE stdinh = 0, stdouth = 0, stderrh = 0, curdirh = 0;
    PROCESS_BASIC_INFORMATION pbi;
    PEB newpeb;
    RTL_USER_PROCESS_PARAMETERS params;
    SECTION_IMAGE_INFORMATION sii;
    IO_STATUS_BLOCK iosb;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    // create section

    status = NtCreateSection(&sect, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE, file);

    if (!success(status)) {
        print("NtCreateSection returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    // get entry point

    status = NtQuerySection(sect, SECTION_INFORMATION_CLASS::ImageInformation, &sii,
                            sizeof(SECTION_IMAGE_INFORMATION), nullptr);

    if (!success(status)) {
        print("NtQuerySection returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(sect);
        return;
    }

    // create process

    status = NtCreateProcess(&proc, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(), false, sect,
                             0, 0);

    NtClose(sect);

    if (!success(status)) {
        print("NtCreateProcess returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    // duplicate handles

    if (peb->ProcessParameters->StandardInput) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->StandardInput, proc,
                                   &stdinh, 0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }
    }

    if (peb->ProcessParameters->StandardOutput) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->StandardOutput, proc,
                                   &stdouth, 0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }
    }

    if (peb->ProcessParameters->StandardError) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->StandardError, proc,
                                   &stderrh, 0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }
    }

    if (peb->ProcessParameters->CurrentDirectory.Handle) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle, proc,
                                   &curdirh, 0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }
    }

    // take copy of PEB

    status = NtQueryInformationProcess(proc, PROCESSINFOCLASS::BasicInformation, &pbi, sizeof(pbi),
                                       nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
        return;
    }

    status = NtReadVirtualMemory(proc, pbi.PebBaseAddress, &newpeb, sizeof(PEB), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
        return;
    }

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc, newpeb.ProcessParameters, &params, sizeof(params), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
        return;
    }

    // change handles

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stderrh;
    params.CurrentDirectory.Handle = curdirh;

    // copy current directory string

    params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                 peb->ProcessParameters->CurrentDirectory.DosPath.Length);

    status = NtWriteVirtualMemory(proc, params.CurrentDirectory.DosPath.Buffer,
                                  peb->ProcessParameters->CurrentDirectory.DosPath.Buffer,
                                  params.CurrentDirectory.DosPath.Length, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
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
        NtClose(proc);
        return;
    }

    // allocate misc block in process

    void* proc_misc_block;
    size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

    status = NtAllocateVirtualMemory(proc, &proc_misc_block, 0, &proc_misc_block_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        free(misc_block);
        NtClose(proc);
        return;
    }

    // copy cmdline

    utf8_to_utf16(span((char16_t*)misc_block, u16_cmdline_len), cmdline);
    misc_block[u16_cmdline_len * sizeof(char16_t)] = 0;
    params.CommandLine.Length = (uint16_t)(u16_cmdline_len * sizeof(char16_t));
    params.CommandLine.MaximumLength = (uint16_t)((u16_cmdline_len + 1) * sizeof(char16_t));
    params.CommandLine.Buffer = (char16_t*)proc_misc_block;

    // copy misc block

    status = NtWriteVirtualMemory(proc, proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        free(misc_block);
        NtClose(proc);
        return;
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc, newpeb.ProcessParameters, &params, sizeof(params), nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
        return;
    }

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000;

    status = NtAllocateVirtualMemory(proc, &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
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

    status = NtCreateThread(&thread, THREAD_ALL_ACCESS, nullptr, proc, &client_id, &context,
                            &initial_teb, true);

    if (!success(status)) {
        print("NtCreateThread returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
        return;
    }

    // mark as foreground process
    status = NtDeviceIoControlFile(peb->ProcessParameters->StandardInput, 0, nullptr, nullptr,
                                   &iosb, IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND,
                                   &proc, sizeof(proc), nullptr, 0);
    auto set_fg = success(status);

    NtResumeThread(thread, nullptr);

    NtClose(thread);

    do {
        status = NtWaitForSingleObject(proc, true, nullptr);

        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print("NtWaitForSingleObject returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }

        break;
    } while (true);

    if (set_fg) {
        HANDLE this_proc = NtCurrentProcess();

        // mark self as foreground process
        status = NtDeviceIoControlFile(peb->ProcessParameters->StandardInput, 0, nullptr, nullptr,
                                       &iosb, IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND,
                                       &this_proc, sizeof(this_proc), nullptr, 0);

        if (!success(status)) {
            print("NtDeviceIoControlFile returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }
    }

    NtClose(proc);
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
        print("NtQuerySystemTime returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    print("System time is ");
    print(time.QuadPart);
    print("\n");

    auto ymd = system_time_to_ymd(time.QuadPart);
    auto hms = system_time_to_hms(time.QuadPart);

    print(ymd);
    print(" ");
    print(hms);
    print("\n");
}

static void timer() {
    HANDLE h;
    NTSTATUS status;
    LARGE_INTEGER due_time;

    status = NtCreateTimer(&h, TIMER_ALL_ACCESS, nullptr, TIMER_TYPE::SynchronizationTimer);
    if (!success(status)) {
        print("NtCreateTimer returned "); // FIXME - should be to stderr
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    due_time.QuadPart = -10000000; // 1 second

    status = NtSetTimer(h, &due_time, nullptr, nullptr, false,
                        1000, nullptr);
    if (!success(status)) {
        print("NtSetTimer returned "); // FIXME - should be to stderr
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(h);
        return;
    }

    for (unsigned int i = 0; i < 3; i++) {
        status = NtWaitForSingleObject(h, false, nullptr);
        if (!success(status)) {
            print("NtWaitForSingleObject returned "); // FIXME - should be to stderr
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(h);
            return;
        }

        print("timer fired\n");
    }

    NtClose(h);
}

static void segfault() {
    *(uint8_t*)0 = 0;
}

static void halt() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownPowerOff);

    // shouldn't return

    print("NtShutdownSystem returned ");
    print_hex((uint32_t)status, 8);
    print("\n");
}

static void reboot() {
    auto status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownReboot);

    // shouldn't return

    print("NtShutdownSystem returned ");
    print_hex((uint32_t)status, 8);
    print("\n");
}

static void try_run_program(string_view program, string_view cmdline) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file;
    IO_STATUS_BLOCK iosb;

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

    status = NtCreateFile(&file, 0, &oa, &iosb, nullptr, 0, 0, FILE_OPEN, 0,
                          nullptr, 0);

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

            status = NtCreateFile(&file, 0, &oa, &iosb, nullptr, 0, 0,
                                  FILE_OPEN, 0, nullptr, 0);
        }
    }

    if (status == nt_status::OBJECT_NAME_NOT_FOUND || status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print(program); // FIXME - stderr
        print(": command not found\n");
        return;
    } else if (!success(status)) {
        print("NtCreateFile returned "); // FIXME - stderr
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    // FIXME - descriptive message if not image
    // FIXME - don't try to run DLLs
    run_program(file, cmdline);

    NtClose(file);
}

static void process_line(string_view sv) {
    if (!sv.empty() && sv.back() == '\r')
        sv = string_view(sv.data(), sv.size() - 1);

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
    else if (cmd == "halt")
        halt();
    else if (cmd == "reboot")
        reboot();
    else
        try_run_program(cmd, sv);
}

static void process_lines() {
    while (true) {
        bool found = false;
        auto sv = string_view(buf, buflen);

        for (unsigned int i = 0; i < sv.size(); i++) {
            if (sv[i] == '\n') {
                process_line(string_view(sv.data(), i));

                auto newsv = string_view(sv.data() + i + 1, sv.size() - i - 1);

                if (newsv.size() > 0) {
                    auto buf2 = (char*)malloc(newsv.size());
                    if (!buf2) {
                        print("out of memory\n");
                        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
                    }

                    memcpy(buf2, newsv.data(), newsv.size());
                    free(buf);
                    buf = buf2;
                } else {
                    free(buf);
                    buf = nullptr;
                }

                buflen = (unsigned int)newsv.size();

                found = true;
                break;
            }
        }

        if (!found)
            return;
    }
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

static void handle_input(string_view sv) {
    bool nl = false;

    for (auto c : sv) {
        if (c == '\b') {
            bool ignore_bs = true;

            if (buflen >= 1 && !(buf[buflen - 1] & 0x80)) {
                buflen--;
                ignore_bs = false;
            } else if (buflen >= 2 && (buf[buflen - 1] & 0xc0) == 0x80) {
                if ((buf[buflen - 2] & 0xe0) == 0xc0) {
                    buflen -= 2;
                    ignore_bs = false;
                } else if (buflen >= 3 && (buf[buflen - 2] & 0xc0) == 0x80) {
                    if ((buf[buflen - 3] & 0xf0) == 0xe0) {
                        buflen -= 3;
                        ignore_bs = false;
                    } else if (buflen >= 4 && (buf[buflen - 3] & 0xc0) == 0x80 && (buf[buflen - 4] & 0xf8) == 0xf0) {
                        buflen -= 4;
                        ignore_bs = false;
                    }
                }
            }

            if (!ignore_bs)
                print("\b");
        } else if (c == '\x04') { // Ctrl+D
            if (buflen == 0) {
                print("\n");
                NtTerminateProcess(NtCurrentProcess(), nt_status::SUCCESS);
            }
        } else {
            if (c == '\n')
                nl = true;

            buf[buflen] = c;
            buflen++;
            print(string_view(&c, 1));
        }
    }

    if (nl) {
        process_lines();
        print("> ");
    }
}

extern "C"
void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

//     thread_test();

    print("> ");

    while (true) {
        IO_STATUS_BLOCK iosb;
        char s[10];

        auto status = NtReadFile(peb->ProcessParameters->StandardInput, 0, nullptr, nullptr, &iosb,
                                 s, sizeof(s), nullptr, nullptr);

        if (!success(status)) {
            print("NtReadFile returned "); // FIXME - should be to stderr
            print_hex((uint32_t)status, 8);
            print("\n");
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

        handle_input(string_view(s, iosb.Information));
    }

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
