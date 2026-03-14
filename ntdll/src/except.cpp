#include "list.h"
#include <stdlib.h>
#include <stdint.h>
#include <optional>
#include <string_view>
#include <span>
#include <utility>
#include "../../modules/exception_record_offsets.h"

import format;
import dwarf;
import print;
import unwind;
import unicode;
import pe;
import kernelbase;
import ntdll;
import ntdll_private;
import crt;

using namespace std;

static_assert(offsetof(EXCEPTION_RECORD, ExceptionCode) == EXCEPTION_RECORD_EXCEPTION_CODE);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionFlags) == EXCEPTION_RECORD_EXCEPTION_FLAGS);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionRecord) == EXCEPTION_RECORD_EXCEPTION_RECORD);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionAddress) == EXCEPTION_RECORD_EXCEPTION_ADDRESS);
static_assert(offsetof(EXCEPTION_RECORD, NumberParameters) == EXCEPTION_RECORD_NUMBER_PARAMETERS);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionInformation) == EXCEPTION_RECORD_EXCEPTION_INFORMATION);

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
                             rva, base, (uintptr_t)ent->ldr.DllBase, sp,
                             mod_name_sv);

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
#elifdef __x86_64__
    auto ent = get_ldr_from_address(context.Rip);
#endif
    if (!ent)
        return false;

    return unwind_context(context, ent->ldr.DllBase, ent->ldr.SizeOfImage);
}

static nt_status handle_exception(EXCEPTION_RECORD* exception,
                                  CONTEXT& orig_context) {
    CONTEXT context;
    DISPATCHER_CONTEXT dispatcher_context;

    context = orig_context;

    while (true) {
        uint32_t handler_addr = 0;
        uint32_t* handler_data = nullptr;
        RUNTIME_FUNCTION* function_entry = nullptr;
        uint64_t frame = 0;

#ifdef __aarch64__
        auto ent = get_ldr_from_address(context.Pc);
#elifdef __x86_64__
        auto ent = get_ldr_from_address(context.Rip);
#endif
        if (!ent)
            break;

        auto prev_context = context;

        if (!unwind_context(context, ent->ldr.DllBase, ent->ldr.SizeOfImage,
                            &handler_addr, &handler_data, &function_entry,
                            &frame)) {
            break;
        }

        // FIXME - should be using RtlVirtualUnwind?

        // FIXME - UNW_FLAG_UHANDLER handlers

        if (!handler_addr)
            continue;

        auto handler = (EXCEPTION_ROUTINE)((uint8_t*)ent->ldr.DllBase + handler_addr);

        // FIXME - aarch64??

#ifdef __x86_64__
        dispatcher_context.ControlPc = prev_context.Rip;
        dispatcher_context.ImageBase = (uint64_t)ent->ldr.DllBase;
        dispatcher_context.FunctionEntry = function_entry;
        dispatcher_context.EstablisherFrame = frame;
        dispatcher_context.TargetIp = 0; // FIXME? (uint64_t)
        dispatcher_context.ContextRecord = &prev_context;
        dispatcher_context.LanguageHandler = handler;
        dispatcher_context.HandlerData = handler_data;
#endif

        auto disp = handler(exception, (void*)frame, &prev_context,
                            &dispatcher_context);

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
    if (success(status)) {
        NtContinue(context, false); // doesn't return
        unreachable();
    }

    if (status != nt_status::UNHANDLED_EXCEPTION) {
        sp = format(sp, "Error {} received while trying to handle exception\n", status);
        print("{}", string_view(s, (size_t)(sp.data() - s)));
    }

    // FIXME - print EXE name

    // FIXME - everything here should be printing to stderr

    if ((NTSTATUS)exception->ExceptionCode == nt_status::CONTROL_C_EXIT) {
        print("\nTerminated\n");
        NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
        unreachable();
    }

    if (exception->ExceptionCode == MOXNOX_STUB_FUNCTION) {
        auto func = string_view((char*)exception->ExceptionInformation[0]);

        print("Call to unimplemented function {}\n", func);
    } else {
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
                            esr.value, +esr.ISS, +esr.IL, +esr.EC, +esr.ISS2);
                break;
            }
#endif

            default:
                sp = format(sp, "Unhandled exception {:08x}\n", exception->ExceptionCode);
            break;
        }
    }

    sp = format(sp, "{}", *context);
    print("{}", string_view(s, (size_t)(sp.data() - s)));

    print("\n");

    do {
#ifdef __aarch64__
        uint64_t rip = context->Pc;
#elifdef __x86_64__
        uint64_t rip = context->Rip;
#endif
        print_rip(rip);

        if (!do_unwind_context(*context))
            break;
    } while (true);

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
    unreachable();
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

extern "C"
EXCEPTION_DISPOSITION __C_specific_handler(EXCEPTION_RECORD* /*exception_record*/,
                                           void* /*establisher_frame*/,
                                           CONTEXT* /*context_record*/,
                                           DISPATCHER_CONTEXT* /*dispatcher_context*/) {
    print("FIXME - __C_specific_handler\n"); // FIXME

    return EXCEPTION_DISPOSITION::ExceptionContinueSearch;
}

#ifdef __x86_64__

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

extern "C"
void RtlUnwindEx(void* target_frame, void* target_ip,
                 EXCEPTION_RECORD* exception_record, void* return_value,
                 CONTEXT* original_context,
                 UNWIND_HISTORY_TABLE* history_table) {
    CONTEXT context = *original_context;
    EXCEPTION_RECORD er;

    if (!exception_record) {
        memset(&er, 0, sizeof(er));
        er.ExceptionCode = (uint32_t)nt_status::UNWIND;
        er.ExceptionFlags = EH_UNWINDING;
        exception_record = &er;
    } else
        exception_record->ExceptionFlags |= EH_UNWINDING;

    while (true) {
        uint64_t image_base, frame;
        void* data = nullptr;

        auto function_entry = RtlLookupFunctionEntry(context.Rip, &image_base,
                                                     history_table);
        if (!function_entry) {
            context.Rip = *(uint64_t*)context.Rsp;
            context.Rsp += 8;

            if (!context.Rip)
                break;

            continue;
        }

        auto& ui = *(UNWIND_INFO*)(image_base + function_entry->UnwindData);

        if (ui.frame_register == 0)
            frame = context.Rsp;
        else {
            frame = get_register(context, (enum unwind_register)ui.frame_register);
            frame -= ui.frame_offset * 16;
        }

        if (target_frame)
            exception_record->ExceptionFlags |= EH_TARGET_UNWIND;

        if (ui.flags & UNW_FLAG_UHANDLER) {
            auto ptr = (uint8_t*)&ui + sizeof(UNWIND_INFO) + (ui.unwind_codes_count * sizeof(UNWIND_CODE));

            if (ui.unwind_codes_count % 2)
                ptr += sizeof(UNWIND_CODE);

            auto handler_addr = *(uint32_t*)ptr;
            auto handler_data = (uint32_t*)(ptr + sizeof(uint32_t));

            auto handler = (EXCEPTION_ROUTINE)(image_base + handler_addr);

            DISPATCHER_CONTEXT dispatcher_context;
            dispatcher_context.ControlPc = context.Rip;
            dispatcher_context.ImageBase = image_base;
            dispatcher_context.FunctionEntry = function_entry;
            dispatcher_context.EstablisherFrame = frame;
            dispatcher_context.TargetIp = (uint64_t)target_ip;
            dispatcher_context.ContextRecord = &context;
            dispatcher_context.LanguageHandler = handler;
            dispatcher_context.HandlerData = handler_data;
            dispatcher_context.HistoryTable = history_table;

            handler(exception_record, (void*)frame, &context,
                    &dispatcher_context);
        }

        if (target_frame && frame == (uint64_t)target_frame)
            break;

        RtlVirtualUnwind(UNW_FLAG_UHANDLER, image_base, context.Rip,
                         function_entry, &context, &data, &frame,
                         nullptr);

        if (context.Rip == 0)
            break;
    }

    context.Rax = (uint64_t)return_value;

    if (target_ip)
        context.Rip = (uint64_t)target_ip;

    NtContinue(&context, false);
}

#endif

static span<const uint8_t> find_pdata(void* img_addr) {
    const auto& dos_header = *(IMAGE_DOS_HEADER*)img_addr;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
        return span<const uint8_t>();

    const auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)img_addr + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE)
        return span<const uint8_t>();

    span<const IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    // FIXME - probably should be looking in data directory rather than going off name

    for (const auto& sect : sections) {
        if (string_view(sect.Name) == ".pdata")
            return span((uint8_t*)img_addr + sect.VirtualAddress, sect.VirtualSize);
    }

    return span<const uint8_t>();
}

#ifdef __x86_64__

extern "C"
RUNTIME_FUNCTION* RtlLookupFunctionEntry(uintptr_t pc, uintptr_t* base,
                                         UNWIND_HISTORY_TABLE* /*history_table*/) {
    void* img_base;

    // FIXME - history_table?

    if (!RtlPcToFileHeader((void*)pc, &img_base))
        return nullptr;

    *base = (uintptr_t)img_base;

    pc -= (uintptr_t)*base;

    // FIXME - how do JITted programs provide their pdata?

    auto pdata = find_pdata(img_base);
    if (pdata.empty())
        return nullptr;

    span<RUNTIME_FUNCTION> rt_funcs((RUNTIME_FUNCTION*)pdata.data(),
                                    pdata.size() / sizeof(RUNTIME_FUNCTION));

    for (auto& f : rt_funcs) {
        if (pc >= f.BeginAddress && pc < f.EndAddress)
            return &f;

        if (f.BeginAddress > pc) // stored in order
            break;
    }

    return nullptr;
}

extern "C"
EXCEPTION_ROUTINE RtlVirtualUnwind(uint32_t type, uint64_t image_base,
                                   uint64_t pc, RUNTIME_FUNCTION* func,
                                   CONTEXT* context, void** handler_data,
                                   uint64_t* establisher_frame,
                                   KNONVOLATILE_CONTEXT_POINTERS* /*ctx_ptr*/) {
    bool machine_frame = false;

    print("FIXME - RtlVirtualUnwind\n");

    auto& ui = *(UNWIND_INFO*)((uint8_t*)image_base + func->UnwindInfoAddress);

    if (ui.version != 1)
        return nullptr;

    uint64_t frame;

    if (ui.frame_register == 0)
        frame = context->Rsp;
    else {
        frame = get_register(*context, (enum unwind_register)ui.frame_register);
        frame -= ui.frame_offset * 16;
    }

    *establisher_frame = frame;

    auto func_offset = (uint32_t)(pc - (uintptr_t)image_base - func->BeginAddress);

    span codes((UNWIND_CODE*)((uint8_t*)image_base + func->UnwindInfoAddress + sizeof(UNWIND_INFO)),
               ui.unwind_codes_count);

    for (size_t i = 0; i < codes.size(); i++) {
        const auto& c = codes[i];

        // FIXME - skip some codes if we're in prologue

        if (c.offset > func_offset)
            continue;

        switch ((enum unwind_opcode)c.code) {
            case unwind_opcode::UWOP_PUSH_NONVOL: { // push reg (.seh_pushreg)
                auto val = read_uint64_t(context->Rsp);

                if (!val.has_value())
                    return nullptr;

                set_register(*context, (enum unwind_register)c.info, *val);
                context->Rsp += sizeof(uint64_t);
                break;
            }

            case unwind_opcode::UWOP_ALLOC_LARGE: // sub rsp, n (.seh_stackalloc)
                if (c.info == 0) {
                    if (i == codes.size() - 1)
                        return nullptr;

                    context->Rsp += *(uint16_t*)&codes[i + 1] * 8;
                    i++;
                } else {
                    if (i >= codes.size() - 2)
                        return nullptr;

                    context->Rsp += *(uint32_t*)&codes[i + 1];
                    i += 2;
                }
                break;

            case unwind_opcode::UWOP_ALLOC_SMALL: // sub rsp, n (.seh_stackalloc)
                context->Rsp += (c.info * 8) + 8;
                break;

            case unwind_opcode::UWOP_SET_FPREG: // lea framereg, [rsp + n] (.seh_setframe)
                context->Rsp = frame;
                break;

            case unwind_opcode::UWOP_SAVE_NONVOL: { // mov [rsp+n], reg (.seh_savereg)
                if (i == codes.size() - 1)
                    return nullptr;

                auto val = read_uint64_t(frame + (*(uint16_t*)&codes[i + 1] * 8));

                if (!val.has_value())
                    return nullptr;

                set_register(*context, (enum unwind_register)c.info, *val);
                i++;
                break;
            }

            case unwind_opcode::UWOP_SAVE_NONVOL_FAR: { // mov [rsp+n], reg (.seh_savereg)
                if (i >= codes.size() - 2)
                    return nullptr;

                auto val = read_uint64_t(frame + *(uint32_t*)&codes[i + 1]);

                if (!val.has_value())
                    return nullptr;

                set_register(*context, (enum unwind_register)c.info, *val);
                i += 2;
                break;
            }

            case unwind_opcode::UWOP_SAVE_XMM128: // movaps [rsp+n], xmmreg (.seh_savexmm)
                if (i == codes.size() - 1)
                    return nullptr;

                // FIXME - ctx_ptr?

                i++;
                break;

            case unwind_opcode::UWOP_SAVE_XMM128_FAR: // movaps [rsp+n], xmmreg (.seh_savexmm)
                if (i >= codes.size() - 2)
                    return nullptr;

                // FIXME - ctx_ptr?

                i += 2;
                break;

            case unwind_opcode::UWOP_PUSH_MACHFRAME: { // interrupt (.seh_pushframe)
                if (c.info != 0)
                    context->Rsp += sizeof(uint64_t);

                auto val = read_uint64_t(context->Rsp);

                if (!val.has_value())
                    return nullptr;

                context->Rip = *val;

                val = read_uint64_t(context->Rsp + (3 * sizeof(uint64_t)));

                if (!val.has_value())
                    return nullptr;

                context->Rsp = *val;
                machine_frame = true;
                break;
            }

            default:
                return nullptr;
        }
    }

    if (!machine_frame) {
        context->Rip = *(uint64_t*)context->Rsp;
        context->Rsp += 8;
    }

    auto ptr = (uint8_t*)&ui + sizeof(UNWIND_INFO) + (ui.unwind_codes_count * sizeof(UNWIND_CODE));

    if (ui.unwind_codes_count % 2)
        ptr += sizeof(UNWIND_CODE);

    *handler_data = (uint32_t*)(ptr + sizeof(uint32_t));

    if (!(ui.flags & type))
        return nullptr;

    // FIXME - also return nullptr if inside prologue

    return (EXCEPTION_ROUTINE)(image_base + *(uint32_t*)ptr);
}

#else

extern "C"
RUNTIME_FUNCTION* RtlLookupFunctionEntry(uintptr_t pc, uintptr_t* base,
                                         UNWIND_HISTORY_TABLE* table) {
    print_stderr("FIXME - RtlLookupFunctionEntry\n"); // FIXME

    return nullptr;
}

#endif
