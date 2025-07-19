#include "ntdll_private.h"
#include "list.h"
#include <utf16_to_utf8.h>
#include <pe.h>
#include <stdlib.h>
#include <optional>
#include "exception_record_offsets.h"

import format;
import print;
import dwarf;
import unwind;

using namespace std;

static_assert(offsetof(EXCEPTION_RECORD, ExceptionAddress) == EXCEPTION_RECORD_EXCEPTION_ADDRESS);

static ldr_entry* get_ldr_from_address(uint64_t addr) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto& ldr_data = *peb.Ldr;
    auto le = ldr_data.InMemoryOrderModuleList.Flink;

    while (le != &ldr_data.InMemoryOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

        if (addr >= (uintptr_t)ent.DllBase && addr < (uintptr_t)ent.DllBase + ent.SizeOfImage)
            return CONTAINING_RECORD(&ent, ldr_entry, ldr);

        le = le->Flink;
    }

    return nullptr;
}

static uint64_t get_preferred_base(ldr_entry& ent) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.ldr.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.ldr.DllBase + dos_header.e_lfanew);

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return nt_header.OptionalHeader64.ImageBase;
    else
        return nt_header.OptionalHeader32.ImageBase;
}

static void print_rip(uint64_t rip) {
    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "{:016x}", rip);

    auto ent = get_ldr_from_address(rip);

    if (ent) {
        // FIXME - also get filename and line no. from DWARF

        sp = format(sp, " ({}!", ent->ldr.BaseDllName);

        auto debug_info = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_info, ent->size_debug_info);
        auto debug_abbrev = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_abbrev, ent->size_debug_abbrev);
        auto debug_rnglists = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_rnglists, ent->size_debug_rnglists);
        auto debug_addr = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_addr, ent->size_debug_addr);
        auto debug_str = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_str, ent->size_debug_str);
        auto debug_str_offsets = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_str_offsets, ent->size_debug_str_offsets);

        auto base = get_preferred_base(*ent);

        auto func_start = dwarf_find_function(debug_info, debug_abbrev, debug_rnglists,
                                              debug_addr, debug_str, debug_str_offsets,
                                              rip - (uintptr_t)ent->ldr.DllBase + base, sp);

        if (func_start != 0) {
            if (rip - (uintptr_t)ent->ldr.DllBase + base != func_start)
                sp = format(sp, "+{:x}", rip - (uintptr_t)ent->ldr.DllBase + base - func_start);
        } else {
            // erase exclamation mark
            sp = span(sp.data() - 1, sp.size() + 1);

            sp = format(sp, "+{:x}", rip - (uintptr_t)ent->ldr.DllBase);
        }

        sp = format(sp, ")");
    }

    sp = format(sp, "\n");

    print("{}", string_view(s, (size_t)(sp.data() - s)));
}

optional<uint64_t> read_uint64_t(uint64_t addr) {
    NTSTATUS status;
    uint64_t ret;
    uint32_t bytes_read;

    status = NtReadVirtualMemory(NtCurrentProcess(), (void*)addr, &ret, sizeof(uint64_t),
                                 &bytes_read);
    if (!success(status) || bytes_read != sizeof(uint64_t))
        return nullopt;

    return ret;
}

static bool do_unwind_context(CONTEXT& context) {
    auto ent = get_ldr_from_address(context.Rip);
    if (!ent)
        return false;

    return unwind_context(context, ent->ldr.DllBase, ent->ldr.SizeOfImage);
}

extern "C"
void mn_signal(EXCEPTION_RECORD* exception, CONTEXT* context) {
    char s[1024];
    auto sp = span(s, sizeof(s));

    // FIXME - call SEH handler if present

    // FIXME - print EXE name

    // FIXME - everything here should be printing to stderr

    if ((NTSTATUS)exception->ExceptionCode == nt_status::CONTROL_C_EXIT)
        print("\nTerminated\n");
    else {
        switch ((NTSTATUS)exception->ExceptionCode) {
            case nt_status::ACCESS_VIOLATION:
                // FIXME - translate error code to descriptive message
                sp = format(sp, "Page fault (address = {:x}, error code = {:x})\n",
                            exception->ExceptionInformation[1], exception->ExceptionInformation[0]);
            break;

            case nt_status::INTEGER_DIVIDE_BY_ZERO:
                sp = format(sp, "Divide by zero\n");
            break;

            case nt_status::ILLEGAL_INSTRUCTION:
                sp = format(sp, "Invalid opcode\n");
            break;

            case nt_status::BREAKPOINT:
                sp = format(sp, "Unhandled breakpoint\n");
            break;

            case nt_status::SINGLE_STEP:
                sp = format(sp, "Unhandled single-step breakpoint\n");
            break;

            default:
                sp = format(sp, "Unhandled exception {:08x}\n", exception->ExceptionCode);
            break;
        }

        sp = format(sp, "{}", *context);
        print("{}", string_view(s, (size_t)(sp.data() - s)));

        // FIXME - DWARF information in stack trace?

        print("\n");

        do {
            print_rip(context->Rip);

            if (!do_unwind_context(*context))
                break;
        } while (true);
    }

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
}
