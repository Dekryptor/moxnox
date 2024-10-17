#include "ntdll_private.h"
#include "list.h"
#include <utf16_to_utf8.h>

using namespace std;

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
        sp = sp_strcpy(sp, ")\n");
    }

    print(string_view(s, sp.data() - s));
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

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
}
