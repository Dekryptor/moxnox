#include "ntdll_private.h"
#include "list.h"
#include <utf16_to_utf8.h>
#include <pe.h>
#include <stdlib.h>

using namespace std;

struct RUNTIME_FUNCTION {
    uint32_t BeginAddress;
    uint32_t EndAddress;
    union {
        uint32_t UnwindInfoAddress;
        uint32_t UnwindData;
    };
};

static LDR_DATA_TABLE_ENTRY* get_ldr_from_address(uint64_t addr) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto& ldr_data = *peb.Ldr;
    auto le = ldr_data.InMemoryOrderModuleList.Flink;

    while (le != &ldr_data.InMemoryOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

        if (addr >= (uintptr_t)ent.DllBase && addr < (uintptr_t)ent.DllBase + ent.SizeOfImage)
            return &ent;

        le = le->Flink;
    }

    return nullptr;
}

static void print_rip(uint64_t rip) {
    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = sp_itoa_hex(sp, rip, 16);

    auto ldr = get_ldr_from_address(rip);

    if (ldr) {
        sp = sp_strcpy(sp, " (");
        sp = sp_strcpy(sp, ldr->BaseDllName);
        sp = sp_strcpy(sp, "+");
        sp = sp_itoa_hex(sp, rip - (uintptr_t)ldr->DllBase);
        sp = sp_strcpy(sp, ")");
    }

    sp = sp_strcpy(sp, "\n");

    print(string_view(s, sp.data() - s));
}

static span<const uint8_t> find_pdata(const LDR_DATA_TABLE_ENTRY& ldr) {
    const auto& dos_header = *(IMAGE_DOS_HEADER*)ldr.DllBase;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
        return span<const uint8_t>();

    const auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ldr.DllBase + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE)
        return span<const uint8_t>();

    span<const IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    for (const auto& sect : sections) {
        if (string_view(sect.Name) == ".pdata")
            return span((uint8_t*)ldr.DllBase + sect.VirtualAddress, sect.VirtualSize);
    }

    return span<const uint8_t>();
}

static const RUNTIME_FUNCTION* find_runtime_function(const LDR_DATA_TABLE_ENTRY& ldr,
                                                     uint64_t rip) {
    rip -= (uintptr_t)ldr.DllBase;

    // FIXME - how do JITted programs provide their pdata?

    auto pdata = find_pdata(ldr);
    if (pdata.empty())
        return nullptr;

    span<RUNTIME_FUNCTION> rt_funcs((RUNTIME_FUNCTION*)pdata.data(),
                                    pdata.size() / sizeof(RUNTIME_FUNCTION));

    for (const auto& f : rt_funcs) {
        if (rip >= f.BeginAddress && rip < f.EndAddress)
            return &f;

        if (f.BeginAddress > rip) // stored in order
            break;
    }

    return nullptr;
}

static bool rewind_context2(CONTEXT& context, const LDR_DATA_TABLE_ENTRY& ldr,
                            const RUNTIME_FUNCTION& rt_func) {
    UNWIND_INFO ui;
    NTSTATUS status;
    uint32_t bytes_read;

    if (rt_func.UnwindInfoAddress > ldr.SizeOfImage - sizeof(UNWIND_INFO))
        return false;

    status = NtReadVirtualMemory(NtCurrentProcess(), (uint8_t*)ldr.DllBase + rt_func.UnwindInfoAddress,
                                 &ui, sizeof(UNWIND_INFO), &bytes_read);
    if (!success(status) || bytes_read != sizeof(UNWIND_INFO))
        return false;

    if (ui.version != 1)
        return false;

    if (ui.unwind_codes_count == 0)
        return true;

    if (rt_func.UnwindInfoAddress > ldr.SizeOfImage - sizeof(UNWIND_INFO) - (ui.unwind_codes_count * sizeof(UNWIND_CODE)))
        return false;

    auto codes = (UNWIND_CODE*)malloc(ui.unwind_codes_count * sizeof(UNWIND_CODE));
    if (!codes)
        return false;

    status = NtReadVirtualMemory(NtCurrentProcess(),
                                 (uint8_t*)ldr.DllBase + rt_func.UnwindInfoAddress + sizeof(UNWIND_INFO),
                                 codes, ui.unwind_codes_count * sizeof(UNWIND_CODE), &bytes_read);
    if (!success(status) || bytes_read != ui.unwind_codes_count * sizeof(UNWIND_CODE)) {
        free(codes);
        return false;
    }

    // FIXME - parse

    free(codes);

    return true;
}

static bool rewind_context(CONTEXT& context) {
    NTSTATUS status;
    uint64_t rip;
    uint32_t bytes_read;

    auto ldr = get_ldr_from_address(context.Rip);
    if (!ldr)
        return false;

    auto rt_func = find_runtime_function(*ldr, context.Rip);

    // RUNTIME_FUNCTION isn't compulsory for leaf functions with no stack
    if (rt_func) {
        if (!rewind_context2(context, *ldr, *rt_func))
            return false;
    }

    status = NtReadVirtualMemory(NtCurrentProcess(), (void*)context.Rsp, &rip,
                                 sizeof(uint64_t), &bytes_read);
    if (!success(status) || bytes_read != sizeof(uint64_t))
        return false;

    context.Rsp -= sizeof(uint64_t);
    context.Rip = rip;

    // FIXME

    return true;
}

extern "C" __declspec(dllexport)
void mn_signal(EXCEPTION_RECORD* exception, CONTEXT* context) {
    char s[1024];
    auto sp = span(s, sizeof(s));

    // FIXME - call SEH handler if present

    // FIXME - print EXE name

    switch ((NTSTATUS)exception->ExceptionCode) {
        case nt_status::ACCESS_VIOLATION:
            // FIXME - translate error code to descriptive message
            sp = sp_strcpy(sp, "Page fault (address = ");
            sp = sp_itoa_hex(sp, exception->ExceptionInformation[1]);
            sp = sp_strcpy(sp, ", error code = ");
            sp = sp_itoa_hex(sp, exception->ExceptionInformation[0]);
            sp = sp_strcpy(sp, ")\n");
        break;

        case nt_status::INTEGER_DIVIDE_BY_ZERO:
            sp = sp_strcpy(sp, "Divide by zero\n");
        break;

        case nt_status::ILLEGAL_INSTRUCTION:
            sp = sp_strcpy(sp, "Invalid opcode\n");
        break;

        default:
            sp = sp_strcpy(sp, "Unhandled exception ");
            sp = sp_itoa_hex(sp, exception->ExceptionCode, 8);
            sp = sp_strcpy(sp, "\n");
        break;
    }

    sp = sp_context(sp, *context);
    print(string_view(s, sp.data() - s));

    // FIXME - print stacktrace? With DWARF information?

    print("\n");

    print_rip(context->Rip);

    if (rewind_context(*context))
        print_rip(context->Rip);

    // FIXME

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
}
