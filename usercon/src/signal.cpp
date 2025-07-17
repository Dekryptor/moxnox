module;

#include <ntdll.h>

import print;
import unique_handle;

export module signal;

static nt_status find_mn_signal(HANDLE proc, uint64_t& rip) {
    nt_status status;
    PROCESS_BASIC_INFORMATION pbi;
    uintptr_t ptr;
    PEB_LDR_DATA pebldr;
    void* ntdll;

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
                                 &ntdll, sizeof(void*), nullptr);
    if (!success(status)) {
        print("NtReadVirtualMemory returned {}\n", status);
        return status;
    }

    // FIXME - find mn_signal export in ntdll

    rip = (uintptr_t)ntdll; // FIXME

    return nt_status::SUCCESS;
}

export void send_ctrl_c(HANDLE proc) {
    HANDLE h;
    unique_handle thread;
    CONTEXT ctx;

    auto status = NtGetNextThread(proc, 0, 0/*FIXME - access*/, 0, 0, &h);

    if (status == nt_status::NO_MORE_ENTRIES)
        return;
    else if (!success(status)) {
        print("NtGetNextThread returned {}\n", status);
        return;
    }

    thread.reset(h);

    // FIXME - call NtGetNextThread until we get a good thread?

    status = NtSuspendThread(thread.get(), nullptr);
    if (!success(status)) {
        print("NtSuspendThread returned {}\n", status);
        return;
    }

    status = NtGetContextThread(thread.get(), &ctx);
    if (!success(status)) {
        print("NtGetContextThread returned {}\n", status);
        return;
    }

    print("thread rip = {:x}\n", ctx.Rip);

    // FIXME - call NtWriteVirtualMemory to put exception record on stack
    // FIXME - call NtWriteVirtualMemory to put context on stack

    status = find_mn_signal(proc, ctx.Rip);
    if (!success(status))
        return;

    // FIXME - set ctx.Rcx to exception
    // FIXME - set ctx.Rdx to context
    // FIXME - adjust ctx.Rsp

    status = NtSetContextThread(thread.get(), &ctx);
    if (!success(status)) {
        print("NtSetContextThread returned {}\n", status);
        return;
    }

    // FIXME - should be NtAlertResumeThread?
    status = NtResumeThread(thread.get(), nullptr);
    if (!success(status)) {
        print("NtResumeThread returned {}\n", status);
        return;
    }

    print("FIXME - send signal to thread\n"); // FIXME
}
