#include "ntdll_private.h"
#include "list.h"
#include <utf16_to_utf8.h>
#include <pe.h>
#include <stdlib.h>
#include <optional>

import format;
import print;
import dwarf;

using namespace std;

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

static void print_rip(uint64_t rip) {
    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "{:16x}", rip);

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

        auto func_start = dwarf_find_function(debug_info, debug_abbrev, debug_rnglists,
                                              debug_addr, debug_str, debug_str_offsets,
                                              rip, sp);

        if (func_start != 0) {
            if (rip != func_start)
                sp = format(sp, "+{:x}", rip - func_start);
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

static optional<uint64_t> read_uint64_t(uint64_t addr) {
    NTSTATUS status;
    uint64_t ret;
    uint32_t bytes_read;

    status = NtReadVirtualMemory(NtCurrentProcess(), (void*)addr, &ret, sizeof(uint64_t),
                                 &bytes_read);
    if (!success(status) || bytes_read != sizeof(uint64_t))
        return nullopt;

    return ret;
}

enum class unwind_opcode {
    UWOP_PUSH_NONVOL = 0,
    UWOP_ALLOC_LARGE = 1,
    UWOP_ALLOC_SMALL = 2,
    UWOP_SET_FPREG = 3,
    UWOP_SAVE_NONVOL = 4,
    UWOP_SAVE_NONVOL_FAR = 5,
    UWOP_SAVE_XMM128 = 8,
    UWOP_SAVE_XMM128_FAR = 9,
    UWOP_PUSH_MACHFRAME = 10,
};

enum class unwind_register {
    rax = 0,
    rcx = 1,
    rdx = 2,
    rbx = 3,
    rsp = 4,
    rbp = 5,
    rsi = 6,
    rdi = 7,
    r8 = 8,
    r9 = 9,
    r10 = 10,
    r11 = 11,
    r12 = 12,
    r13 = 13,
    r14 = 14,
    r15 = 15
};

static uint64_t get_register(const CONTEXT& context, enum unwind_register reg) {
    switch (reg) {
        case unwind_register::rax:
            return context.Rax;
        case unwind_register::rcx:
            return context.Rcx;
        case unwind_register::rdx:
            return context.Rdx;
        case unwind_register::rbx:
            return context.Rbx;
        case unwind_register::rsp:
            return context.Rsp;
        case unwind_register::rbp:
            return context.Rbp;
        case unwind_register::rsi:
            return context.Rsi;
        case unwind_register::rdi:
            return context.Rdi;
        case unwind_register::r8:
            return context.R8;
        case unwind_register::r9:
            return context.R9;
        case unwind_register::r10:
            return context.R10;
        case unwind_register::r11:
            return context.R11;
        case unwind_register::r12:
            return context.R12;
        case unwind_register::r13:
            return context.R13;
        case unwind_register::r14:
            return context.R14;
        case unwind_register::r15:
            return context.R15;
    }

    return 0;
}

static void set_register(CONTEXT& context, enum unwind_register reg, uint64_t value) {
    switch (reg) {
        case unwind_register::rax:
            context.Rax = value;
            break;
        case unwind_register::rcx:
            context.Rcx = value;
            break;
        case unwind_register::rdx:
            context.Rdx = value;
            break;
        case unwind_register::rbx:
            context.Rbx = value;
            break;
        case unwind_register::rsp:
            context.Rsp = value;
            break;
        case unwind_register::rbp:
            context.Rbp = value;
            break;
        case unwind_register::rsi:
            context.Rsi = value;
            break;
        case unwind_register::rdi:
            context.Rdi = value;
            break;
        case unwind_register::r8:
            context.R8 = value;
            break;
        case unwind_register::r9:
            context.R9 = value;
            break;
        case unwind_register::r10:
            context.R10 = value;
            break;
        case unwind_register::r11:
            context.R11 = value;
            break;
        case unwind_register::r12:
            context.R12 = value;
            break;
        case unwind_register::r13:
            context.R13 = value;
            break;
        case unwind_register::r14:
            context.R14 = value;
            break;
        case unwind_register::r15:
            context.R15 = value;
            break;
    }
}

static bool unwind_context2(CONTEXT& context, const LDR_DATA_TABLE_ENTRY& ldr,
                            const RUNTIME_FUNCTION* prt_func) {
    UNWIND_INFO ui;
    NTSTATUS status;
    uint32_t bytes_read;
    uint32_t func_offset;
    uint64_t frame = context.Rsp;
    RUNTIME_FUNCTION rt_func2;

    while (true) {
        const auto& rt_func = *prt_func;

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

        if (ui.unwind_codes_count > 0) {
            if (rt_func.UnwindInfoAddress > ldr.SizeOfImage - sizeof(UNWIND_INFO) - (ui.unwind_codes_count * sizeof(UNWIND_CODE)))
                return false;

            auto codesptr = (UNWIND_CODE*)malloc(ui.unwind_codes_count * sizeof(UNWIND_CODE));
            if (!codesptr)
                return false;

            if (context.Rip >= (uintptr_t)ldr.DllBase + rt_func.BeginAddress &&
                context.Rip < (uintptr_t)ldr.DllBase + rt_func.BeginAddress + ui.prologue_bytes) {
                func_offset = (uint32_t)(context.Rip - (uintptr_t)ldr.DllBase - rt_func.BeginAddress);
            } else
                func_offset = 0xffffffff;

            span codes(codesptr, ui.unwind_codes_count);

            status = NtReadVirtualMemory(NtCurrentProcess(),
                                        (uint8_t*)ldr.DllBase + rt_func.UnwindInfoAddress + sizeof(UNWIND_INFO),
                                        codesptr, ui.unwind_codes_count * sizeof(UNWIND_CODE), &bytes_read);
            if (!success(status) || bytes_read != ui.unwind_codes_count * sizeof(UNWIND_CODE)) {
                free(codesptr);
                return false;
            }

            if (ui.frame_register != 0) {
                frame = get_register(context, (enum unwind_register)ui.frame_register);
                frame -= ui.frame_offset * 16;
            }

            for (size_t i = 0; i < codes.size(); i++) {
                const auto& c = codes[i];

                if (c.offset > func_offset)
                    continue;

                switch ((enum unwind_opcode)c.code) {
                    case unwind_opcode::UWOP_PUSH_NONVOL: { // push reg (.seh_pushreg)
                        auto val = read_uint64_t(context.Rsp);

                        if (!val.has_value()) {
                            free(codesptr);
                            return false;
                        }

                        set_register(context, (enum unwind_register)c.info, *val);
                        context.Rsp += sizeof(uint64_t);
                        break;
                    }

                    case unwind_opcode::UWOP_ALLOC_LARGE: // sub rsp, n (.seh_stackalloc)
                        if (c.info == 0) {
                            if (i == codes.size() - 1) {
                                free(codesptr);
                                return false;
                            }

                            context.Rsp += *(uint16_t*)&codes[i + 1] * 8;
                            i++;
                        } else {
                            if (i >= codes.size() - 2) {
                                free(codesptr);
                                return false;
                            }

                            context.Rsp += *(uint32_t*)&codes[i + 1];
                            i += 2;
                        }
                        break;

                    case unwind_opcode::UWOP_ALLOC_SMALL: // sub rsp, n (.seh_stackalloc)
                        context.Rsp += (c.info * 8) + 8;
                        break;

                    case unwind_opcode::UWOP_SET_FPREG: // lea framereg, [rsp + n] (.seh_setframe)
                        context.Rsp = frame;
                        break;

                    case unwind_opcode::UWOP_SAVE_NONVOL: // mov [rsp+n], reg (.seh_savereg)
                        if (i == codes.size() - 1) {
                            free(codesptr);
                            return false;
                        }

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint16_t*)&codes[i + 1] * 8);
                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_NONVOL_FAR: // mov [rsp+n], reg (.seh_savereg)
                        if (i >= codes.size() - 2) {
                            free(codesptr);
                            return false;
                        }

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint32_t*)&codes[i + 1]);
                        i += 2;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i == codes.size() - 1) {
                            free(codesptr);
                            return false;
                        }

                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128_FAR: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i >= codes.size() - 2) {
                            free(codesptr);
                            return false;
                        }

                        i += 2;
                        break;

                    case unwind_opcode::UWOP_PUSH_MACHFRAME: // interrupt (.seh_pushframe)
                        print("FIXME - handle UWOP_PUSH_MACHFRAME\n");
                        break;

                    default: {
                        print("unrecognized unwind opcode {:x}\n", c.code);
                        free(codesptr);
                        return false;
                    }
                }
            }

            free(codesptr);
        }

        if (!(ui.flags & UNW_FLAG_CHAININFO))
            break;

        uint8_t* addr = (uint8_t*)ldr.DllBase;
        addr += rt_func.UnwindInfoAddress;
        addr += ui.unwind_codes_count * sizeof(UNWIND_CODE);

        if (ui.unwind_codes_count & 1)
            addr += sizeof(UNWIND_CODE);

        status = NtReadVirtualMemory(NtCurrentProcess(), addr, &rt_func2,
                                     sizeof(RUNTIME_FUNCTION), &bytes_read);
        if (!success(status) || bytes_read != sizeof(RUNTIME_FUNCTION))
            return false;

        prt_func = &rt_func2;
    }

    return true;
}

static bool unwind_context(CONTEXT& context) {
    auto ent = get_ldr_from_address(context.Rip);
    if (!ent)
        return false;

    auto rt_func = find_runtime_function(ent->ldr, context.Rip);

    // RUNTIME_FUNCTION isn't compulsory for leaf functions with no stack
    if (rt_func) {
        if (!unwind_context2(context, ent->ldr, rt_func))
            return false;
    }

    auto rip = read_uint64_t(context.Rsp);

    if (!rip.has_value())
        return false;

    context.Rsp += sizeof(uint64_t);
    context.Rip = *rip;

    return true;
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
                sp = format(sp, "Unhandled exception {:8x}\n", exception->ExceptionCode);
            break;
        }

        sp = format(sp, "{}", *context);
        print("{}", string_view(s, (size_t)(sp.data() - s)));

        // FIXME - DWARF information in stack trace?

        print("\n");

        do {
            print_rip(context->Rip);

            if (!unwind_context(*context))
                break;
        } while (true);
    }

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
}
