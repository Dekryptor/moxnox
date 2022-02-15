#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nt.h>
#include <ntdll.h>
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

    print(string_view(s, sp.data() - s));
}

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

static void run_program(u16string_view fn) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file, sect, proc, thread;
    IO_STATUS_BLOCK iosb;
    CLIENT_ID client_id;
    CONTEXT context;
    INITIAL_TEB initial_teb;
    HANDLE stdinh = 0, stdouth = 0, stderrh = 0, curdirh = 0;
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

    // FIXME - copy current directory string
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

    NtResumeThread(thread, nullptr);

    NtClose(thread);

    status = NtWaitForSingleObject(proc, false, nullptr);

    if (!success(status)) {
        print("NtWaitForSingleObject returned ");
        print_hex((uint32_t)status, 8);
        print("\n");
        NtClose(proc);
        return;
    }

    print("finished\n");

    NtClose(proc);
}

extern "C"
void startup() {
    // FIXME - start services etc.

    while (true) {
        // FIXME - solicit username and password
        // FIXME - also launch shell on serial port if available
        run_program(u"\\shell.exe");
    }
}
