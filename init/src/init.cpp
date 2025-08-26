#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nt.h>
#include <ntdll.h>
#include <ioctl.h>
#include <span>
#include <string_view>

using namespace std;

import print;
import cmdline;

static const size_t PAGE_SIZE = 4096;
static const uint64_t PAGE_SHIFT = 12;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static void run_program(u16string_view fn, u16string_view cmdline, HANDLE stdinh,
                        HANDLE stdouth, HANDLE stderrh) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file, sect, proc, thread;
    IO_STATUS_BLOCK iosb;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    HANDLE proc_stdinh = 0, proc_stdouth = 0, proc_stderrh = 0, curdirh = 0;
    PROCESS_BASIC_INFORMATION pbi;
    PEB newpeb;
    RTL_USER_PROCESS_PARAMETERS params;
    SECTION_IMAGE_INFORMATION sii;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    // open file

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)malloc(us.Length);

    if (!us.Buffer) {
        print("malloc failed\n");
        return;
    }

    memcpy(us.Buffer, fn.data(), fn.size() * sizeof(char16_t));

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&file, FILE_READ_DATA, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, 0, nullptr, 0);

    free(us.Buffer);

    if (!success(status)) {
        print("NtCreateFile returned {}\n", status);
        return;
    }

    // create section

    status = NtCreateSection(&sect, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE, file);

    NtClose(file);

    if (!success(status)) {
        print("NtCreateSection returned {}\n", status);
        return;
    }

    // get entry point

    status = NtQuerySection(sect, SECTION_INFORMATION_CLASS::ImageInformation, &sii,
                            sizeof(SECTION_IMAGE_INFORMATION), nullptr);

    if (!success(status)) {
        print("NtQuerySection returned {}\n", status);
        NtClose(sect);
        return;
    }

    // create process

    status = NtCreateProcess(&proc, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(), false, sect,
                             0, 0);

    NtClose(sect);

    if (!success(status)) {
        print("NtCreateProcess returned {}\n", status);
        return;
    }

    // duplicate handles

    if (stdinh) {
        status = NtDuplicateObject(NtCurrentProcess(), stdinh, proc, &proc_stdinh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            NtClose(proc);
            return;
        }
    }

    if (stdouth) {
        status = NtDuplicateObject(NtCurrentProcess(), stdouth, proc, &proc_stdouth, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            NtClose(proc);
            return;
        }
    }

    if (stderrh) {
        status = NtDuplicateObject(NtCurrentProcess(), stderrh, proc, &proc_stderrh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            NtClose(proc);
            return;
        }
    }

    if (peb->ProcessParameters->CurrentDirectory.Handle) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle, proc,
                                   &curdirh, 0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            NtClose(proc);
            return;
        }
    }

    // take copy of PEB

    status = NtQueryInformationProcess(proc, PROCESSINFOCLASS::BasicInformation, &pbi, sizeof(pbi),
                                       nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        NtClose(proc);
        return;
    }

    status = NtReadVirtualMemory(proc, pbi.PebBaseAddress, &newpeb, sizeof(PEB), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        NtClose(proc);
        return;
    }

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc, newpeb.ProcessParameters, &params, sizeof(params), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        NtClose(proc);
        return;
    }

    // change handles

    params.StandardInput = proc_stdinh;
    params.StandardOutput = proc_stdouth;
    params.StandardError = proc_stderrh;
    params.CurrentDirectory.Handle = curdirh;

    // copy current directory string

    params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                 peb->ProcessParameters->CurrentDirectory.DosPath.Length);

    // FIXME - environment variables

    // allocate misc block

    size_t misc_block_size = (cmdline.size() + 1) * sizeof(char16_t);

    auto misc_block = (uint8_t*)malloc(misc_block_size);
    if (!misc_block) {
        print("malloc failed\n");
        NtClose(proc);
        return;
    }

    // allocate misc block in process

    void* proc_misc_block = nullptr;
    size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

    status = NtAllocateVirtualMemory(proc, &proc_misc_block, 0,
                                     &proc_misc_block_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        free(misc_block);
        NtClose(proc);
        return;
    }

    // copy cmdline

    memcpy(misc_block, cmdline.data(), cmdline.size() * sizeof(char16_t));
    misc_block[cmdline.size() * sizeof(char16_t)] = 0;
    params.CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
    params.CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
    params.CommandLine.Buffer = (char16_t*)proc_misc_block;

    // copy misc block

    status = NtWriteVirtualMemory(proc, proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        free(misc_block);
        NtClose(proc);
        return;
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc, newpeb.ProcessParameters, &params, sizeof(params), nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        NtClose(proc);
        return;
    }

    status = NtWriteVirtualMemory(proc, params.CurrentDirectory.DosPath.Buffer,
                                  peb->ProcessParameters->CurrentDirectory.DosPath.Buffer,
                                  params.CurrentDirectory.DosPath.Length, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
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
        print("NtAllocateVirtualMemory returned {}\n", status);
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
        print("NtCreateThread returned {}\n", status);
        NtClose(proc);
        return;
    }

    NtResumeThread(thread, nullptr);

    NtClose(thread);

    do {
        status = NtWaitForSingleObject(proc, true, nullptr);

        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print("NtWaitForSingleObject returned {}\n", status);
            NtClose(proc);
            return;
        }

        break;
    } while (true);

    NtClose(proc);
}

static void do_serial() {
    NTSTATUS status;
    UNICODE_STRING us;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    // FIXME - should be soliciting username and password

    static const char16_t dev[] = u"\\dev\\ttyS0";

    us.Length = us.MaximumLength = (uint16_t)(sizeof(dev) - sizeof(char16_t));
    us.Buffer = (char16_t*)dev;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, 0, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::SHARING_VIOLATION) {
        NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
        return;
    }

    if (!success(status)) {
        print("NtCreateFile returned {}\n", status);
        NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
        return;
    }

    while (true) {
        run_program(u"\\usercon.exe", u"usercon.exe serial", h, h, h);
    }

    NtClose(h);

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}

static void create_thread(void(*func)()) {
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
        print("NtAllocateVirtualMemory returned {}\n", status); // FIXME - should be to stderr
        return;
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
    context.Rip = (uintptr_t)func;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x20;
    context.EFlags = 0x200;
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, NtCurrentProcess(), &client_id, &context,
                            &initial_teb, true);

    if (!success(status)) {
        print("NtCreateThread returned {}\n", status); // FIXME - should be to stderr
        return;
    }

    NtResumeThread(h, nullptr);

    NtClose(h);
}

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
    bool serial_mode = false;

    // FIXME - start services etc.

    {
        auto teb = NtCurrentTeb();
        auto peb = teb->ProcessEnvironmentBlock;

        auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                    peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

        auto argc = count_cmdline_args(cmdline);
        span<u16string_view> argv;
        u16string_view* buf = nullptr;

        if (argc != 0) {
            buf = (decltype(buf))RtlAllocateHeap(GetProcessHeap(), 0, argc * sizeof(*buf));
            if (!buf)
                NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);

            argv = span(buf, argc);
        }

        parse_cmdline(cmdline, argv);

        if (!argv.empty())
            argv = argv.subspan(1);

        for (auto arg : argv) {
            if (arg.empty())
                continue;

            static const u16string_view opt = u"console=";

            if (arg.size() >= opt.size() && !memcmp(arg.data(), opt.data(), opt.size()))
                serial_mode = true;
        }

        if (buf)
            RtlFreeHeap(GetProcessHeap(), 0, buf);
    }

    if (serial_mode) {
        while (true) {
            auto teb = NtCurrentTeb();
            auto peb = teb->ProcessEnvironmentBlock;

            run_program(u"\\usercon.exe", u"usercon.exe serial",
                        peb->ProcessParameters->StandardInput,
                        peb->ProcessParameters->StandardOutput,
                        peb->ProcessParameters->StandardError);
        }
    } else {
        create_thread(do_serial);

        while (true) {
            auto teb = NtCurrentTeb();
            auto peb = teb->ProcessEnvironmentBlock;

            run_program(u"\\usercon.exe", u"usercon.exe",
                        peb->ProcessParameters->StandardInput,
                        peb->ProcessParameters->StandardOutput,
                        peb->ProcessParameters->StandardError);
        }
    }
}
