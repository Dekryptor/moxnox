#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nt.h>
#include <ntdll.h>
#include <ioctl.h>
#include <span>
#include <string_view>

using namespace std;

void print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardOutput, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

span<char> sp_itoa_hex(span<char> dest, uint64_t num, unsigned int padding) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0 && padding == 0) {
        dest[0] = '0';
        return dest.subspan(1);
    }

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || padding == 16 - i || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                dest[0] = (char)(n + '0');
            else
                dest[0] = (char)(n - 0xa + 'a');

            dest = dest.subspan(1);
            first = false;

            if (dest.empty())
                return dest;
        }

        num <<= 4;
    }

    return dest;
}

void print_hex(uint64_t num, unsigned int padding) {
    char s[17];
    span sp(s, sizeof(s));

    sp = sp_itoa_hex(sp, num, padding);

    print(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

static void run_program(u16string_view fn, HANDLE stdinh, HANDLE stdouth, HANDLE stderrh) {
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

    status = NtCreateFile(&file, 0, &oa, &iosb, nullptr, 0, 0, FILE_OPEN, 0, nullptr, 0);

    free(us.Buffer);

    if (!success(status)) {
        print("NtCreateFile returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        return;
    }

    // create section

    status = NtCreateSection(&sect, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE, file);

    NtClose(file);

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

    if (stdinh) {
        status = NtDuplicateObject(NtCurrentProcess(), stdinh, proc, &proc_stdinh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }
    }

    if (stdouth) {
        status = NtDuplicateObject(NtCurrentProcess(), stdouth, proc, &proc_stdouth, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            NtClose(proc);
            return;
        }
    }

    if (stderrh) {
        status = NtDuplicateObject(NtCurrentProcess(), stderrh, proc, &proc_stderrh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

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

    params.StandardInput = proc_stdinh;
    params.StandardOutput = proc_stdouth;
    params.StandardError = proc_stderrh;
    params.CurrentDirectory.Handle = curdirh;

    // copy current directory string

    params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                 peb->ProcessParameters->CurrentDirectory.DosPath.Length);

    // FIXME - environment variables

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc, newpeb.ProcessParameters, &params, sizeof(params), nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
        return;
    }

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

    if (stdinh) {
        NtDeviceIoControlFile(stdinh, 0, nullptr, nullptr,
                              &iosb, IOCTL_MOXNOX_CONSOLE_SET_FOREGROUND,
                              &proc, sizeof(proc), nullptr, 0);
    }

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

    status = NtCreateFile(&h, 0, &oa, &iosb, nullptr, 0, 0, FILE_OPEN, 0, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::SHARING_VIOLATION) {
        NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
        return;
    }

    if (!success(status)) {
        print("NtCreateFile returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
        return;
    }

    while (true) {
        run_program(u"\\shell.exe", h, h, h);
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
    context.Rip = (uintptr_t)func;
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

    NtResumeThread(h, nullptr);

    NtClose(h);
}

extern "C"
void startup() {
    // FIXME - start services etc.

    create_thread(do_serial);

    while (true) {
        auto teb = NtCurrentTeb();
        auto peb = teb->ProcessEnvironmentBlock;

        // FIXME - solicit username and password

        print("\nWelcome to \e[1mMoxnox\e[m\n\n");

        run_program(u"\\shell.exe", peb->ProcessParameters->StandardInput,
                    peb->ProcessParameters->StandardOutput, peb->ProcessParameters->StandardError);
    }
}
