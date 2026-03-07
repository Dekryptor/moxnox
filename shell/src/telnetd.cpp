#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string_view>
#include <bit>
#include <utility>
#include <span>
#include <tuple>

#undef errno
import errno;

import nt;
import ws2_32;
import print;
import ipv6;
import unique_handle;
import pe;
import pagesize;
import kernelbase;
import format;

using namespace std;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static bool run_program(u16string_view exe, u16string_view cmdline,
                        HANDLE curdir, unique_handle& proc,
                        HANDLE client_handle) {
    HANDLE h, stdinh = 0, stdouth, curdirh = 0;
    unique_handle file, sect, thread;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    SECTION_IMAGE_INFORMATION sii;
    PROCESS_BASIC_INFORMATION pbi;
    RTL_USER_PROCESS_PARAMETERS params;
    PEB newpeb;
    INITIAL_TEB initial_teb;
    CONTEXT context;
    CLIENT_ID client_id;
    nt_status status;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    // open file

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(exe.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)exe.data();

    status = NtOpenFile(&h, FILE_READ_DATA, &oa, &iosb, 0,
                        FILE_NON_DIRECTORY_FILE);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return false;
    }

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());
    if (!success(status)) {
        print("NtCreateSection returned {}\n", status);
        return false;
    }

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation,
                            &sii, sizeof(SECTION_IMAGE_INFORMATION), nullptr);

    if (!success(status)) {
        print("NtQuerySection returned {}\n", status);
        return false;
    }

    if (sii.ImageCharacteristics & IMAGE_FILE_DLL) {
        print("{}: not an executable\n", exe);
        return false;
    }

#ifdef __aarch64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_ARM64;
#elifdef __x86_64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_AMD64;
#endif

    if (sii.Machine != this_arch) {
        print("{}: unsupported architecture\n", exe);
        return false;
    }

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect.get(), 0, 0);
    if (!success(status)) {
        print("NtCreateProcess returned {}\n", status);
        return false;
    }

    proc.reset(h);

    // duplicate handles

    status = NtDuplicateObject(NtCurrentProcess(), client_handle,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);

    if (!success(status)) {
        print("NtDuplicateObject returned {}\n", status);
        return false;
    }

    status = NtDuplicateObject(NtCurrentProcess(), client_handle,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);

    if (!success(status)) {
        print("NtDuplicateObject returned {}\n", status);
        return false;
    }

    if (curdir != 0) {
        status = NtDuplicateObject(NtCurrentProcess(), curdir, proc.get(),
                                   &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            return false;
        }
    } else if (peb->ProcessParameters->CurrentDirectory.Handle) {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle,
                                   proc.get(), &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);

        if (!success(status)) {
            print("NtDuplicateObject returned {}\n", status);
            return false;
        }
    }

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return false;
    }

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return false;
    }

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return false;
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
        return false;
    }

    // allocate misc block

    size_t misc_block_size = (cmdline.size() + 1) * sizeof(char16_t);

    misc_block_size += peb->ProcessParameters->EnvironmentSize;

    auto misc_block = (uint8_t*)malloc(misc_block_size);
    if (!misc_block) {
        print("malloc failed\n");
        return false;
    }

    // allocate misc block in process

    void* proc_misc_block = nullptr;
    size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

    status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                     &proc_misc_block_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        free(misc_block);
        return false;
    }

    // copy cmdline

    auto ptr = misc_block;

    memcpy(misc_block, cmdline.data(), cmdline.size() * sizeof(char16_t));
    ptr += cmdline.size() * sizeof(char16_t);

    *ptr = 0;
    ptr++;

    params.CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
    params.CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
    params.CommandLine.Buffer = (char16_t*)proc_misc_block;

    memcpy(ptr, peb->ProcessParameters->Environment,
           peb->ProcessParameters->EnvironmentSize);
    params.Environment = (uint8_t*)proc_misc_block + (ptr - misc_block);
    params.EnvironmentSize = peb->ProcessParameters->EnvironmentSize;

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block,
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        free(misc_block);
        return false;
    }

    free(misc_block);

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                  sizeof(params), nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        return false;
    }

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000; // FIXME - should come from PE header?

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        return false;
    }

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = (uintptr_t)sii.TransferAddress;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10; // 0x10 bytes for red zone
#elifdef __x86_64__
    context.Rip = (uintptr_t)sii.TransferAddress;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc.get(),
                            &client_id, &context, &initial_teb, true);
    if (!success(status)) {
        print("NtCreateThread returned {}\n", status);
        return false;
    }

    thread.reset(h);

    NtResumeThread(thread.get(), nullptr);

    return true;
}

static void print_to_sock2(SOCKET sock, string_view msg) {
    auto msg2 = span(msg.data(), msg.size());

    while (true) {
        nt_status status;
        IO_STATUS_BLOCK iosb;

        status = NtWriteFile((HANDLE)sock, 0, nullptr, nullptr, &iosb, msg2.data(),
                                (uint32_t)msg2.size(), nullptr, nullptr);
        if (status == nt_status::END_OF_FILE) {
            closesocket(sock);
            NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
            unreachable();
        } else if (!success(status)) {
            print_stderr("NtWriteFile returned {}\n", status);
            closesocket(sock);
            NtTerminateThread(NtCurrentThread(), status);
            unreachable();
        }

        if (iosb.Information == msg2.size())
            break;

        msg2 = span(msg2.data() + iosb.Information,
                    msg2.size() - iosb.Information);
    }
}

template<typename... Args>
void print_to_sock(SOCKET sock, type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];

    auto sp = span(buf, sizeof(buf));

    sp = format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    print_to_sock2(sock, sv);
}

static uint32_t socket_thread(void* ctx) {
    auto sock = (SOCKET)(uintptr_t)ctx;
    unique_handle proc;

    // FIXME - this isn't really telnet, telnet has an actual protocol

    // FIXME - get curdir from HOME?
    if (!run_program(u"/bin/shell.exe", u"/bin/shell.exe", 0, proc,
                     (HANDLE)sock)) {
        print_to_sock(sock, "Unable to start shell\n");
        closesocket(sock);
        return 1;
    }

    closesocket(sock);
    return 0;
}

int main() {
    struct sockaddr_in6 addr;

    auto server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_sock < 0) {
        print_stderr("socket returned {} (errno {})\n", server_sock, errno_to_string_view(*_errno()));
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = byteswap((uint16_t)23);

    int ret = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret) {
        print_stderr("bind returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    ret = listen(server_sock, 5);
    if (ret) {
        print_stderr("listen returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    print("Telnet server listening on port 23...\n");

    while (true) {
        struct sockaddr_in6 client_addr;
        int client_addrlen = sizeof(client_addr);

        auto client_sock = accept(server_sock, (struct sockaddr*)&client_addr,
                                  &client_addrlen);
        if (client_sock < 0) {
            print_stderr("accept returned {} (errno {})\n", client_sock,
                         errno_to_string_view(*_errno()));
            closesocket(server_sock);
            return 1;
        }

        if (client_addrlen != sizeof(client_addr)) {
            print_stderr("client_addrlen was {}, expected {}\n", client_addrlen,
                         sizeof(client_addr));
            closesocket(client_sock);
            closesocket(server_sock);
            return 1;
        }

        print("Connected from [{}]:{}, socket {}\n", *(ipv6_address*)client_addr.sin6_addr,
              byteswap(client_addr.sin6_port), (uintptr_t)client_sock);

        if (!CreateThread(nullptr, 0, socket_thread, (void*)(uintptr_t)client_sock,
                          0, nullptr)) {
            print_stderr("CreateThread failed (last error {})\n", GetLastError());
            closesocket(client_sock);
        }
    }

    closesocket(server_sock);

    return 0;
}
