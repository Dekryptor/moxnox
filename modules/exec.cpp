module;

#include <string_view>
#include <span>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

export module exec;

import nt;
import ntdll;
import unique_handle;
import unicode;
import pagesize;

#undef errno
import errno;

using namespace std;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static size_t get_cmdline_len(char* const* argv) {
    size_t ret = 0;

    // FIXME - quotes?

    for (size_t i = 0; argv[i]; i++) {
        if (i != 0)
            ret++;

        ret += utf8_to_utf16_len(argv[i]);
    }

    return ret;
}

static void do_cmdline(span<char16_t> buf, char* const* argv) {
    // FIXME - quotes?

    for (size_t i = 0; argv[i]; i++) {
        if (i != 0) {
            buf[0] = u' ';
            buf = span(buf.data() + 1, buf.size() - 1);
        }

        auto sv = string_view(argv[i]);
        auto len = utf8_to_utf16_len(sv);

        utf8_to_utf16(buf, sv);

        buf = span(buf.data() + len, buf.size() - len);
    }

    buf[0] = 0;
}

static size_t get_environ_len(char* const* envp) {
    size_t ret = 0;

    for (size_t i = 0; envp[i]; i++) {
        ret += utf8_to_utf16_len(envp[i]) + 1;
    }

    ret++;

    return ret;
}

static void do_environ(span<char16_t> buf, char* const* envp) {
    for (size_t i = 0; envp[i]; i++) {
        auto sv = string_view(envp[i]);
        auto len = utf8_to_utf16_len(sv);

        utf8_to_utf16(buf, sv);

        buf = span(buf.data() + len, buf.size() - len);

        buf[0] = 0;
        buf = span(buf.data() + 1, buf.size() - 1);
    }

    buf[0] = 0;
}

export int do_exec(const char* path, char* const* argv, char* const* envp,
                   unique_handle& proc, unique_handle& thread,
                   CLIENT_ID& client_id) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle file, sect;
    PROCESS_BASIC_INFORMATION pbi;
    SECTION_IMAGE_INFORMATION sii;
    RTL_USER_PROCESS_PARAMETERS params;
    PEB newpeb;
    INITIAL_TEB initial_teb;
    CONTEXT context;
    nt_status status;
    HANDLE stdinh, stdouth, stderrh, curdirh;
    uintptr_t our_ntdll, their_ntdll;
    ANSI_STRING as;
    void* our_mn_exec;
    uintptr_t their_mn_exec;

    static const string_view mn_exec = "mn_exec";

    string_view pathsv = path;
    auto u16pathlen = utf8_to_utf16_len(pathsv);

    auto u16path = (char16_t*)malloc(u16pathlen * sizeof(char16_t));
    if (!u16path)
        return ENOMEM;

    utf8_to_utf16(span(u16path, u16pathlen), pathsv);

    auto u16pathsv = u16string_view(u16path, u16pathlen);

    // open file

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(u16pathsv.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)u16pathsv.data();

    status = NtOpenFile(&h, FILE_READ_DATA, &oa, &iosb, 0,
                        FILE_NON_DIRECTORY_FILE);

    free(u16path);

    if (!success(status))
        return nt_status_to_errno(status);

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());
    if (!success(status))
        return nt_status_to_errno(status);

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation,
                            &sii, sizeof(SECTION_IMAGE_INFORMATION), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect.get(), 0, 0);
    if (!success(status))
        return nt_status_to_errno(status);

    proc.reset(h);

    // duplicate standard handles (FIXME - these should actually be our fd 0, 1, and 2?)

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->StandardInput,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->StandardOutput,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->StandardError,
                               proc.get(), &stderrh, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtDuplicateObject(NtCurrentProcess(), peb.ProcessParameters->CurrentDirectory.Handle,
                               proc.get(), &curdirh, 0, 0,
                               DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status))
        return nt_status_to_errno(status);

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // change handles and current directory in PEB

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stderrh;
    params.CurrentDirectory.Handle = curdirh;

    // FIXME - CurrentDirectory.DosPath?

    // allocate misc block

    auto cmdline_len = get_cmdline_len(argv);
    auto env_len = get_environ_len(envp);

    size_t misc_block_size = (cmdline_len + 1 + env_len) * sizeof(char16_t);
    uint8_t* misc_block;
    void* proc_misc_block = nullptr;

    misc_block = (uint8_t*)malloc(misc_block_size);
    if (!misc_block)
        return ENOMEM;

    // allocate misc block in process

    size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

    status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                     &proc_misc_block_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        free(misc_block);
        return nt_status_to_errno(status);
    }

    // copy cmdline

    span sp = span((char16_t*)misc_block, misc_block_size / sizeof(char16_t));

    do_cmdline(sp, argv);

    params.CommandLine.Length = (uint16_t)(cmdline_len * sizeof(char16_t));
    params.CommandLine.MaximumLength = (uint16_t)((cmdline_len + 1) * sizeof(char16_t));
    params.CommandLine.Buffer = (char16_t*)proc_misc_block;

    sp = span(sp.data() + (params.CommandLine.MaximumLength / sizeof(char16_t)),
              sp.size() - (params.CommandLine.MaximumLength / sizeof(char16_t)));

    do_environ(sp, envp);

    params.Environment = (uint8_t*)proc_misc_block + params.CommandLine.MaximumLength;
    params.EnvironmentSize = env_len * sizeof(char16_t);

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        free(misc_block);
        return nt_status_to_errno(status);
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                  sizeof(params), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // find their ntdll

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxFindNtdll,
                                       &their_ntdll, sizeof(their_ntdll), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    // find our ntdll

    status = NtQueryInformationProcess(NtCurrentProcess(), PROCESS_INFORMATION_CLASS::MoxnoxFindNtdll,
                                       &our_ntdll, sizeof(our_ntdll), nullptr);
    if (!success(status))
        return nt_status_to_errno(status);

    as.Buffer = (char*)mn_exec.data();
    as.Length = as.MaximumLength = (uint16_t)mn_exec.size();

    status = LdrGetProcedureAddress((void*)our_ntdll, &as, 0, &our_mn_exec);
    if (!success(status))
        return nt_status_to_errno(status);

    their_mn_exec = (uintptr_t)our_mn_exec - our_ntdll + their_ntdll;

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000; // FIXME - should come from PE header?

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status))
        return nt_status_to_errno(status);

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = their_mn_exec;
    context.X0 = (uintptr_t)sii.TransferAddress;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10;
#elifdef __x86_64__
    context.Rip = their_mn_exec;
    context.Rcx = (uintptr_t)sii.TransferAddress;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc.get(),
                            &client_id, &context, &initial_teb, true);
    if (!success(status))
        return nt_status_to_errno(status);

    thread.reset(h);

    return 0;
}
