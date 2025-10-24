module;

#include <string_view>
#include <span>
#include <optional>
#include <tuple>
#include <string.h>
#include <stdint.h>

export module signal;

import nt;
import ntdll;
import unique_handle;
import pe;

using namespace std;

export bool send_ctrl_c(HANDLE proc) {
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
    if (!success(status))
        return false;

    status = NtReadVirtualMemory(proc, (uint8_t*)pbi.PebBaseAddress + offsetof(PEB, Ldr),
                                 &ptr, sizeof(uintptr_t), nullptr);
    if (!success(status))
        return false;

    status = NtReadVirtualMemory(proc, (void*)ptr, &pebldr, sizeof(pebldr),
                                 nullptr);
    if (!success(status))
        return false;

    // search for ntdll (first entry in InLoadOrderModuleList, in our implementation)

    uintptr_t list_head = ptr + offsetof(PEB_LDR_DATA, InLoadOrderModuleList);

    auto le = (uintptr_t)pebldr.InLoadOrderModuleList.Flink;
    if (le == list_head)
        return false;

    status = NtReadVirtualMemory(proc, (void*)(le - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks) + offsetof(LDR_DATA_TABLE_ENTRY, DllBase)),
                                 &their_ntdll, sizeof(uintptr_t), nullptr);
    if (!success(status))
        return false;

    auto& ldr = *(LDR_DATA_TABLE_ENTRY*)((uint8_t*)peb->Ldr->InLoadOrderModuleList.Flink - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks));

    our_ntdll = (uintptr_t)ldr.DllBase;

    ansiname.Buffer = (char*)name.data();
    ansiname.Length = ansiname.MaximumLength = (uint16_t)name.size();

    status = LdrGetProcedureAddress((void*)our_ntdll, &ansiname, 0, &addr);
    if (!success(status))
        return false;

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    size_t stack_size = 0x100000; // FIXME - should come from PE header?

    status = NtAllocateVirtualMemory(proc, &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status))
        return false;

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = (uintptr_t)addr;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10; // 0x10 for red zone
#else
    context.Rip = (uintptr_t)addr;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x20 - sizeof(uint64_t); // red zone, plus (unused) return pointer
    context.EFlags = 0x200;
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;
#endif

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc,
                            &client_id, &context, &initial_teb, true);
    if (!success(status))
        return false;

    thread.reset(h);

    NtResumeThread(thread.get(), nullptr);

    return true;
}
