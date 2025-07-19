module;

#include <nt.h>
#include <pe.h>
#include <span>
#include <string_view>
#include <optional>

export module unwind;

using namespace std;

extern "C++" optional<uint64_t> read_uint64_t(uint64_t addr);

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

static const RUNTIME_FUNCTION* find_runtime_function(void* img_addr, uint64_t rip) {
    rip -= (uintptr_t)img_addr;

    // FIXME - how do JITted programs provide their pdata?

    auto pdata = find_pdata(img_addr);
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

static bool unwind_context2(CONTEXT& context, void* img_addr, size_t img_size,
                            const RUNTIME_FUNCTION* prt_func,
                            bool& machine_frame, UNWIND_INFO** unwind_info) {
    uint32_t func_offset;
    uint64_t frame = context.Rsp;

    while (true) {
        const auto& rt_func = *prt_func;

        if (rt_func.UnwindInfoAddress > img_size - sizeof(UNWIND_INFO))
            return false;

        auto& ui = *(UNWIND_INFO*)((uint8_t*)img_addr + rt_func.UnwindInfoAddress);

        if (ui.version != 1)
            return false;

        if (ui.unwind_codes_count == 0)
            return true;

        if (ui.unwind_codes_count > 0) {
            if (rt_func.UnwindInfoAddress > img_size - sizeof(UNWIND_INFO) - (ui.unwind_codes_count * sizeof(UNWIND_CODE)))
                return false;

            if (context.Rip >= (uintptr_t)img_addr + rt_func.BeginAddress &&
                context.Rip < (uintptr_t)img_addr + rt_func.BeginAddress + ui.prologue_bytes) {
                func_offset = (uint32_t)(context.Rip - (uintptr_t)img_addr - rt_func.BeginAddress);
            } else
                func_offset = 0xffffffff;

            span codes((UNWIND_CODE*)((uint8_t*)img_addr + rt_func.UnwindInfoAddress + sizeof(UNWIND_INFO)),
                       ui.unwind_codes_count);

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

                        if (!val.has_value())
                            return false;

                        set_register(context, (enum unwind_register)c.info, *val);
                        context.Rsp += sizeof(uint64_t);
                        break;
                    }

                    case unwind_opcode::UWOP_ALLOC_LARGE: // sub rsp, n (.seh_stackalloc)
                        if (c.info == 0) {
                            if (i == codes.size() - 1)
                                return false;

                            context.Rsp += *(uint16_t*)&codes[i + 1] * 8;
                            i++;
                        } else {
                            if (i >= codes.size() - 2)
                                return false;

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
                        if (i == codes.size() - 1)
                            return false;

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint16_t*)&codes[i + 1] * 8);
                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_NONVOL_FAR: // mov [rsp+n], reg (.seh_savereg)
                        if (i >= codes.size() - 2)
                            return false;

                        set_register(context, (enum unwind_register)c.info,
                                    frame + *(uint32_t*)&codes[i + 1]);
                        i += 2;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i == codes.size() - 1)
                            return false;

                        i++;
                        break;

                    case unwind_opcode::UWOP_SAVE_XMM128_FAR: // movaps [rsp+n], xmmreg (.seh_savexmm)
                        if (i >= codes.size() - 2)
                            return false;

                        i += 2;
                        break;

                    case unwind_opcode::UWOP_PUSH_MACHFRAME: { // interrupt (.seh_pushframe)
                        if (c.info != 0)
                            context.Rsp += sizeof(uint64_t);

                        auto val = read_uint64_t(context.Rsp);

                        if (!val.has_value())
                            return false;

                        context.Rip = *val;

                        val = read_uint64_t(context.Rsp + (3 * sizeof(uint64_t)));

                        if (!val.has_value())
                            return false;

                        context.Rsp = *val;
                        machine_frame = true;
                        break;
                    }

                    default:
                        return false;
                }
            }
        }

        if (unwind_info)
            *unwind_info = &ui;

        if (!(ui.flags & UNW_FLAG_CHAININFO))
            break;

        uint8_t* addr = (uint8_t*)img_addr;
        addr += rt_func.UnwindInfoAddress;
        addr += ui.unwind_codes_count * sizeof(UNWIND_CODE);

        if (ui.unwind_codes_count & 1)
            addr += sizeof(UNWIND_CODE);

        prt_func = (RUNTIME_FUNCTION*)addr;
    }

    return true;
}

export bool unwind_context(CONTEXT& context, void* img_addr, size_t img_size,
                           UNWIND_INFO** unwind_info) {
    bool machine_frame = false;

    auto rt_func = find_runtime_function(img_addr, context.Rip);

    // RUNTIME_FUNCTION isn't compulsory for leaf functions with no stack
    if (rt_func) {
        if (!unwind_context2(context, img_addr, img_size, rt_func,
                             machine_frame, unwind_info)) {
            return false;
        }
    }

    if (!machine_frame) {
        auto rip = read_uint64_t(context.Rsp);

        if (!rip.has_value())
            return false;

        context.Rsp += sizeof(uint64_t);
        context.Rip = *rip;
    }

    return true;
}
