module;

#include <ntdll.h>
#include <pe.h>
#include <string_view>
#include <span>
#include <optional>
#include <tuple>
#include <string.h>

export module signal;

import format;
import unique_handle;

using namespace std;

extern "C++" void do_print(string_view sv);

template<typename... Args>
static void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char s[100];
    auto sp = span(s, sizeof(s));

    sp = format_to(sp, fmt.sv, make_tuple(args...));

    do_print(string_view(s, (size_t)(sp.data() - s)));
}

#if 0
static nt_status find_export(HANDLE proc, uintptr_t dll_base,
                             string_view name, uint64_t& rip) {
    nt_status status;
    uint32_t e_lfanew;
    IMAGE_NT_HEADERS nt_header;
    IMAGE_DATA_DIRECTORY data_dir;

    status = NtReadVirtualMemory(proc, (uint8_t*)dll_base + offsetof(IMAGE_DOS_HEADER, e_lfanew),
                                 &e_lfanew, sizeof(e_lfanew), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return status;
    }

    status = NtReadVirtualMemory(proc, (uint8_t*)dll_base + e_lfanew,
                                 &nt_header, sizeof(nt_header), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return status;
    }

    uintptr_t dir_addr;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
            print("image has no export directory\n");
            return nt_status::INTERNAL_ERROR;
        }

        dir_addr = e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader64.DataDirectory);
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
            print("image has no export directory\n");
            return nt_status::INTERNAL_ERROR;
        }

        dir_addr = e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader32.DataDirectory);
    }

    dir_addr += IMAGE_DIRECTORY_ENTRY_EXPORT * sizeof(IMAGE_DATA_DIRECTORY);

    status = NtReadVirtualMemory(proc, (uint8_t*)dll_base + dir_addr,
                                 &data_dir, sizeof(data_dir), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return status;
    }

    if (data_dir.VirtualAddress == 0 || data_dir.Size == 0) {
        print("image has no export directory\n");
        return nt_status::INTERNAL_ERROR;
    }

    // search export directory

    auto dir = (uint8_t*)RtlAllocateHeap(GetProcessHeap(), 0, data_dir.Size);
    if (!dir) {
        print("out of memory\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtReadVirtualMemory(proc, (uint8_t*)dll_base + data_dir.VirtualAddress,
                                 dir, data_dir.Size, nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        RtlFreeHeap(GetProcessHeap(), 0, dir);
        return status;
    }

    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)dir;

    if (ied.NumberOfNames == 0) {
        print("image has no exports\n");
        RtlFreeHeap(GetProcessHeap(), 0, dir);
        return nt_status::INTERNAL_ERROR;
    }

    auto names = span((uint32_t*)(dir + ied.AddressOfNames - data_dir.VirtualAddress),
                      ied.NumberOfNames);

    if ((uint8_t*)names.data() + names.size_bytes() > dir + data_dir.Size) {
        print("invalid export directory\n");
        RtlFreeHeap(GetProcessHeap(), 0, dir);
        return nt_status::INTERNAL_ERROR;
    }

    auto name_ordinals = span((uint16_t*)(dir + ied.AddressOfNameOrdinals - data_dir.VirtualAddress),
                              ied.NumberOfNames);

    if ((uint8_t*)name_ordinals.data() + name_ordinals.size_bytes() > dir + data_dir.Size) {
        print("invalid export directory\n");
        RtlFreeHeap(GetProcessHeap(), 0, dir);
        return nt_status::INTERNAL_ERROR;
    }

    auto funcs = span((uint32_t*)(dir + ied.AddressOfFunctions - data_dir.VirtualAddress),
                      ied.NumberOfFunctions);

    if ((uint8_t*)funcs.data() + funcs.size_bytes() > dir + data_dir.Size) {
        print("invalid export directory\n");
        RtlFreeHeap(GetProcessHeap(), 0, dir);
        return nt_status::INTERNAL_ERROR;
    }

    optional<uint16_t> ord;

    for (unsigned int i = 0; i < names.size(); i++) {
        auto n = names[i];

        if (n - data_dir.VirtualAddress > data_dir.Size) {
            print("invalid export directory\n");
            RtlFreeHeap(GetProcessHeap(), 0, dir);
            return nt_status::INTERNAL_ERROR;
        }

        auto sv = string_view((char*)dir + n - data_dir.VirtualAddress,
                              data_dir.Size + data_dir.VirtualAddress - n);

        auto nulpos = sv.find_first_of((char)0);

        if (nulpos != string_view::npos)
            sv = string_view(sv.data(), nulpos);

        if (sv == name) {
            ord = name_ordinals[i];
            break;
        }
    }

    if (!ord) {
        print("could not find export {}\n", name);
        RtlFreeHeap(GetProcessHeap(), 0, dir);
        return nt_status::INTERNAL_ERROR;
    }

    if (*ord >= ied.NumberOfFunctions) {
        print("function ordinal out of range\n");
        RtlFreeHeap(GetProcessHeap(), 0, dir);
        return nt_status::INTERNAL_ERROR;
    }

    rip = dll_base + funcs[*ord];

    RtlFreeHeap(GetProcessHeap(), 0, dir);

    return nt_status::SUCCESS;
}
#endif

static nt_status find_mn_signal(HANDLE proc, uint64_t& rip) {
    nt_status status;
    PROCESS_BASIC_INFORMATION pbi;
    uintptr_t ptr;
    PEB_LDR_DATA pebldr;
    uintptr_t ntdll;
    ANSI_STRING ansiname;
    void* addr;

    static const string_view name = "mn_signal";

    // read Ldr from PEB

    status = NtQueryInformationProcess(proc, PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return status;
    }

    status = NtReadVirtualMemory(proc, (uint8_t*)pbi.PebBaseAddress + offsetof(PEB, Ldr),
                                 &ptr, sizeof(uintptr_t), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return status;
    }

    status = NtReadVirtualMemory(proc, (void*)ptr, &pebldr, sizeof(pebldr),
                                 nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return status;
    }

    // search for ntdll (first entry in InLoadOrderModuleList, in our implementation)

    uintptr_t list_head = ptr + offsetof(PEB_LDR_DATA, InLoadOrderModuleList);

    auto le = (uintptr_t)pebldr.InLoadOrderModuleList.Flink;
    if (le == list_head) {
        print("process InLoadOrderModuleList was empty\n");
        return nt_status::INTERNAL_ERROR;
    }

    status = NtReadVirtualMemory(proc, (void*)(le - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks) + offsetof(LDR_DATA_TABLE_ENTRY, DllBase)),
                                 &ntdll, sizeof(uintptr_t), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return status;
    }

    ansiname.Buffer = (char*)name.data();
    ansiname.Length = ansiname.MaximumLength = (uint16_t)name.size();

    // FIXME - address of ntdll

    status = LdrGetProcedureAddress((void*)ntdll, &ansiname, 0, &addr);

    rip = (uint64_t)addr;

    return status;
}

export void send_signal_to_process(HANDLE proc, EXCEPTION_RECORD& er) {
    HANDLE thread;
    CONTEXT ctx;

    auto status = NtGetNextThread(proc, 0, 0/*FIXME - access*/, 0, 0, &thread);

    if (status == nt_status::NO_MORE_ENTRIES)
        return;
    else if (!success(status)) {
        print("NtGetNextThread returned {}\n", status);
        return;
    }

    // FIXME - call NtGetNextThread until we get a good thread?

    status = NtSuspendThread(thread, nullptr);
    if (!success(status)) {
        print("NtSuspendThread returned {}\n", status);
        NtClose(thread);
        return;
    }

    status = NtGetContextThread(thread, &ctx);
    if (!success(status)) {
        print("NtGetContextThread returned {}\n", status);
        NtClose(thread);
        return;
    }

    // make sure CONTEXT gets aligned on 16-byte boundary

    uintptr_t proc_context = (ctx.Rsp & ~(uint64_t)0xf) - sizeof(CONTEXT);

    status = NtWriteVirtualMemory(proc, (void*)proc_context, &ctx,
                                  sizeof(CONTEXT), nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        NtClose(thread);
        return;
    }

    er.ExceptionAddress = (void*)ctx.Rip;

    size_t er_size = offsetof(EXCEPTION_RECORD, ExceptionInformation) + (sizeof(uintptr_t) * er.NumberParameters);
    uintptr_t proc_er = proc_context - er_size;

    status = NtWriteVirtualMemory(proc, (void*)proc_er, &er, (uint32_t)er_size,
                                  nullptr);
    if (!success(status)) {
        print("NtWriteVirtualMemory returned {}\n", status);
        NtClose(thread);
        return;
    }

    status = find_mn_signal(proc, ctx.Rip);
    if (!success(status)) {
        NtClose(thread);
        return;
    }

    ctx.Rcx = proc_er;
    ctx.Rdx = proc_context;

    ctx.Rsp &= ~(uint64_t)0xf;
    ctx.Rsp -= sizeof(CONTEXT);
    ctx.Rsp -= er_size;

    status = NtSetContextThread(thread, &ctx);
    if (!success(status)) {
        print("NtSetContextThread returned {}\n", status);
        NtClose(thread);
        return;
    }

    // FIXME - should be NtAlertResumeThread?
    status = NtResumeThread(thread, nullptr);
    if (!success(status)) {
        print("NtResumeThread returned {}\n", status);
        NtClose(thread);
        return;
    }

    NtClose(thread);
}

export void send_ctrl_c(HANDLE proc) {
    nt_status status;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    PROCESS_BASIC_INFORMATION pbi;
    uintptr_t ptr;
    PEB_LDR_DATA pebldr;
    ANSI_STRING ansiname;
    uintptr_t their_ntdll, our_ntdll;
    void* addr;
    INITIAL_TEB initial_teb;
    CONTEXT context;
    HANDLE h;
    unique_handle thread;
    CLIENT_ID client_id;

    // FIXME - 32-bit processes

    static const string_view name = "mn_ctrl_c";

    // read Ldr from PEB

    status = NtQueryInformationProcess(proc, PROCESSINFOCLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print("NtQueryInformationProcess returned {}\n", status);
        return;
    }

    status = NtReadVirtualMemory(proc, (uint8_t*)pbi.PebBaseAddress + offsetof(PEB, Ldr),
                                 &ptr, sizeof(uintptr_t), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    }

    status = NtReadVirtualMemory(proc, (void*)ptr, &pebldr, sizeof(pebldr),
                                 nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    }

    // search for ntdll (first entry in InLoadOrderModuleList, in our implementation)

    uintptr_t list_head = ptr + offsetof(PEB_LDR_DATA, InLoadOrderModuleList);

    auto le = (uintptr_t)pebldr.InLoadOrderModuleList.Flink;
    if (le == list_head) {
        print("process InLoadOrderModuleList was empty\n");
        return;
    }

    status = NtReadVirtualMemory(proc, (void*)(le - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks) + offsetof(LDR_DATA_TABLE_ENTRY, DllBase)),
                                 &their_ntdll, sizeof(uintptr_t), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return;
    }

    auto& ldr = *(LDR_DATA_TABLE_ENTRY*)((uint8_t*)peb->Ldr->InLoadOrderModuleList.Flink - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks));

    our_ntdll = (uintptr_t)ldr.DllBase;

    ansiname.Buffer = (char*)name.data();
    ansiname.Length = ansiname.MaximumLength = (uint16_t)name.size();

    status = LdrGetProcedureAddress((void*)our_ntdll, &ansiname, 0, &addr);
    if (!success(status)) {
        print("LdrGetProcedureAddress returned {} for {}\n", status, name);
        return;
    }

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000; // FIXME - should come from PE header?

    status = NtAllocateVirtualMemory(proc, &stack, 0, &stack_size,
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
    context.Rip = (uintptr_t)addr;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x20;
    context.EFlags = 0x200;
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc,
                            &client_id, &context, &initial_teb, true);
    if (!success(status)) {
        print("NtCreateThread returned {}\n", status);
        return;
    }

    thread.reset(h);

    NtResumeThread(thread.get(), nullptr);
}
