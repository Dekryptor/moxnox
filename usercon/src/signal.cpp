module;

#include <ntdll.h>
#include <pe.h>
#include <string_view>
#include <span>
#include <optional>

import print;
import unique_handle;

export module signal;

using namespace std;

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

static nt_status find_mn_signal(HANDLE proc, uint64_t& rip) {
    nt_status status;
    PROCESS_BASIC_INFORMATION pbi;
    uintptr_t ptr;
    PEB_LDR_DATA pebldr;
    uintptr_t ntdll;

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

    return find_export(proc, ntdll, "mn_signal", rip);
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
