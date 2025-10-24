#include "ntdll_private.h"
#include "list.h"
#include <kernelbase.h>
#include <stdlib.h>
#include <optional>
#include "exception_record_offsets.h"

import format;
import dwarf;
import print;
import unwind;
import unicode;
import pe;

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

    auto ent = get_ldr_from_address(rip);

    if (ent) {
        char mod_name[255];
        uint64_t rva = rip - (uintptr_t)ent->ldr.DllBase;

        // FIXME - also get filename and line no. from DWARF

        auto u16sv_mod_name = u16string_view(ent->ldr.BaseDllName.Buffer, ent->ldr.BaseDllName.Length / sizeof(char16_t));

        utf16_to_utf8(mod_name, u16sv_mod_name);

        auto mod_name_sv = string_view(mod_name, utf16_to_utf8_len(u16sv_mod_name));

        auto debug_info = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_info, ent->size_debug_info);
        auto debug_abbrev = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_abbrev, ent->size_debug_abbrev);
        auto debug_rnglists = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_rnglists, ent->size_debug_rnglists);
        auto debug_addr = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_addr, ent->size_debug_addr);
        auto debug_str = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_str, ent->size_debug_str);
        auto debug_str_offsets = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_str_offsets, ent->size_debug_str_offsets);
        auto debug_line = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_line,
                               ent->size_debug_line);
        auto debug_line_str = span((uint8_t*)ent->ldr.DllBase + ent->offset_debug_line_str,
                                   ent->size_debug_line_str);

        auto base = get_preferred_base(*ent);

        dwarf_print_function(debug_info, debug_abbrev, debug_rnglists,
                             debug_addr, debug_str, debug_str_offsets,
                             rva, base, sp, mod_name_sv);

        sp = format(sp, " ");

        dwarf_print_line_no(debug_line, debug_line_str, debug_str,
                            debug_str_offsets, rva + base, sp);

        sp = format(sp, "\n");
    } else
        sp = format(sp, "{:016x}\n", rip);

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
#ifdef __aarch64__
    auto ent = get_ldr_from_address(context.Pc);
#else
    auto ent = get_ldr_from_address(context.Rip);
#endif
    if (!ent)
        return false;

    return unwind_context(context, ent->ldr.DllBase, ent->ldr.SizeOfImage);
}

static nt_status handle_exception(EXCEPTION_RECORD* exception,
                                  CONTEXT& orig_context) {
    CONTEXT context;

    context = orig_context;

    while (true) {
        uint32_t handler_addr = 0;

#ifdef __aarch64__
        auto ent = get_ldr_from_address(context.Pc);
#else
        auto ent = get_ldr_from_address(context.Rip);
#endif
        if (!ent)
            break;

        auto prev_context = context;

        if (!unwind_context(context, ent->ldr.DllBase, ent->ldr.SizeOfImage,
                            &handler_addr)) {
            break;
        }

        // FIXME - should be using RtlVirtualUnwind?

        // FIXME - UNW_FLAG_UHANDLER handlers

        if (!handler_addr)
            continue;

        auto handler = (EXCEPTION_ROUTINE)((uint8_t*)ent->ldr.DllBase + handler_addr);

        // FIXME - frame
        // FIXME - dispatcher context
        auto disp = handler(exception, nullptr, &prev_context, nullptr);

        switch (disp) {
            case EXCEPTION_DISPOSITION::ExceptionContinueExecution:
                // FIXME - check for EH_NONCONTINUABLE
                memcpy(&orig_context, &prev_context, sizeof(CONTEXT));
                return nt_status::SUCCESS;

            case EXCEPTION_DISPOSITION::ExceptionContinueSearch:
                break;

            case EXCEPTION_DISPOSITION::ExceptionNestedException:
                print("FIXME - ExceptionNestedException\n");
                break;

            case EXCEPTION_DISPOSITION::ExceptionCollidedUnwind:
                print("FIXME - ExceptionCollidedUnwind\n");
                break;

            default:
                return nt_status::INVALID_DISPOSITION;
        }
    }

    return nt_status::UNHANDLED_EXCEPTION;
}

extern "C"
void mn_signal(EXCEPTION_RECORD* exception, CONTEXT* context) {
    char s[1024];
    auto sp = span(s, sizeof(s));

    auto status = handle_exception(exception, *context);
    if (success(status))
        NtContinue(context, false); // doesn't return

    if (status != nt_status::UNHANDLED_EXCEPTION) {
        sp = format(sp, "Error {} received while trying to handle exception\n", status);
        print("{}", string_view(s, (size_t)(sp.data() - s)));
    }

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

            case nt_status::BAD_STACK:
                sp = format(sp, "Stack alignment error\n");
            break;

#ifdef __aarch64__
            case nt_status::UNHANDLED_EXCEPTION: {
                union ARM64_ESR {
                    struct {
                        uint64_t ISS : 25;
                        uint64_t IL : 1;
                        uint64_t EC : 6;
                        uint64_t ISS2 : 24;
                    };
                    uint64_t value;
                } __attribute((packed)) esr;

                esr.value = exception->ExceptionInformation[0];

                sp = format(sp, "Unhandled exception (ESR_EL1 = {:08x} [ISS = {:x}, IL = {:x}, EC = {:x}, ISS2 = {:x}])\n",
                            esr.value, esr.ISS, esr.IL, esr.EC, esr.ISS2);
                break;
            }
#endif

            default:
                sp = format(sp, "Unhandled exception {:08x}\n", exception->ExceptionCode);
            break;
        }

        sp = format(sp, "{}", *context);
        print("{}", string_view(s, (size_t)(sp.data() - s)));

        // FIXME - DWARF information in stack trace?

        print("\n");

        do {
#ifdef __aarch64__
            uint64_t rip = context->Pc;
#else
            uint64_t rip = context->Rip;
#endif
            print_rip(rip);

            if (!do_unwind_context(*context))
                break;
        } while (true);
    }

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
}

extern "C"
void mn_ctrl_c() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    optional<uintptr_t> kernelbase;
    ANSI_STRING ansiname;
    uint32_t (*func)(void*);

    static const string_view name = "CtrlRoutine";

    for (auto le = peb->Ldr->InLoadOrderModuleList.Flink; le != &peb->Ldr->InLoadOrderModuleList; le = le->Flink) {
        auto& ldr = *(LDR_DATA_TABLE_ENTRY*)((uint8_t*)le - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks));

        auto sv = u16string_view(ldr.BaseDllName.Buffer, ldr.BaseDllName.Length / sizeof(char16_t));

        if (sv == u"kernelbase.dll") {
            kernelbase = (uintptr_t)ldr.DllBase;
            break;
        }
    }

    if (!kernelbase.has_value()) {
        print("could not find kernelbase.dll in process\n");
        NtTerminateThread(NtCurrentThread(), nt_status::CONTROL_C_EXIT);
        return;
    }

    ansiname.Buffer = (char*)name.data();
    ansiname.Length = ansiname.MaximumLength = (uint16_t)name.size();

    auto status = LdrGetProcedureAddress((void*)*kernelbase, &ansiname, 0, (void**)&func);
    if (!success(status)) {
        print("LdrGetProcedureAddress returned {} looking for {} in kernelbase\n",
              status, name);
        NtTerminateThread(NtCurrentThread(), nt_status::CONTROL_C_EXIT);
        return;
    }

    func((void*)CTRL_C_EVENT);

    NtTerminateThread(NtCurrentThread(), nt_status::CONTROL_C_EXIT);
}
