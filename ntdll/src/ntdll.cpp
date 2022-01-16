#include "ntdll.h"
#include "../../kernel/src/pe.h"
#include <optional>
#include <nt.h>

using namespace std;

extern uint8_t __image_base__;

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}

static void print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

static void print_hex(uint64_t num, unsigned int padding = 0) {
    char s[17];
    span sp(s, sizeof(s));

    sp = sp_itoa_hex(sp, num, padding);

    print(string_view(s, sp.data() - s));
}

static optional<uintptr_t> find_export(void* base, string_view name) {
    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    const auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    const auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT) {
            exp_dir = nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            found = true;
        }
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT) {
            exp_dir = nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            found = true;
        }
    }

    if (!found || exp_dir.VirtualAddress == 0)
        return nullopt;

    if (exp_dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) {
        print("invalid export directory\n");
        return nullopt;
    }

    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)((uint8_t*)base + exp_dir.VirtualAddress);

    if (ied.NumberOfNames == 0)
        return nullopt;

    auto names = span((uint32_t*)((uint8_t*)base + ied.AddressOfNames), ied.NumberOfNames);

    auto name_ordinals = span((uint16_t*)((uint8_t*)base + ied.AddressOfNameOrdinals), ied.NumberOfNames);

    auto funcs = span((uint32_t*)((uint8_t*)base + ied.AddressOfFunctions), ied.NumberOfFunctions);

    optional<uint16_t> ord;

    for (unsigned int i = 0; i < names.size(); i++) {
        auto n = names[i];

        auto sv = string_view((char*)base + n);

        if (sv == name) {
            ord = name_ordinals[i];
            break;
        }
    }

    if (!ord)
        return nullopt;

    return funcs[*ord];
}

template<typename T>
static void resolve_imports_dll(void* base, string_view name, T* lookup_table, T* thunks) {
    if (name != "ntdll.dll") { // FIXME - case-insensitive match
        print("FIXME - load DLLs\n"); // FIXME - die if can't find
        return;
    }

    auto dll_base = (void*)&__image_base__;

    while (*lookup_table) {
        auto v = *lookup_table;

        if (v & ((T)1 << ((sizeof(T) * 8) - 1))) {
            print("FIXME - imports by ordinal\n"); // FIXME
            lookup_table++;
            thunks++;
            continue;
        }

        // FIXME - don't ignore hint
        auto func = string_view((char*)base + (v & 0x7fffffff) + sizeof(char16_t));

        auto addr = find_export(dll_base, func);
        if (!addr) {
            print("Could not resolve ");
            print(func);
            print(" in ");
            print(name);
            print(".\n");
            // FIXME - terminate process
            return;
        }

        *thunks = (T)((uintptr_t)dll_base + *addr);

        lookup_table++;
        thunks++;
    }
}

static void resolve_imports(void* base) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            exp_dir = nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            found = true;
        }
    }

    if (!found || exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
        return;

    auto iids = span((IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)base + exp_dir.VirtualAddress),
                     exp_dir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR));

    for (auto& iid : iids) {
        if (iid.Characteristics == 0)
            break;

        auto name = string_view((char*)base + iid.Name);

        if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            resolve_imports_dll(base, name,
                                (uint64_t*)((uint8_t*)base + iid.Characteristics),
                                (uint64_t*)((uint8_t*)base + iid.FirstThunk));
        } else {
            resolve_imports_dll(base, name,
                                (uint32_t*)((uint8_t*)base + iid.Characteristics),
                                (uint32_t*)((uint8_t*)base + iid.FirstThunk));
        }
    }
}

static void resolve_relocations(void* base) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    uint64_t header_base;
    span<uint8_t> dir;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return;
        }

        dir = span((uint8_t*)base + nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress,
                   nt_header.OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

        header_base = nt_header.OptionalHeader64.ImageBase;
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return;
        }

        dir = span((uint8_t*)base + nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress,
                   nt_header.OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

        header_base = nt_header.OptionalHeader32.ImageBase;
    }

    if ((uintptr_t)base == header_base) // not relocated
        return;

    do {
        auto& reloc = *(IMAGE_BASE_RELOCATION*)dir.data();

        if (dir.size() < reloc.SizeOfBlock)
            break;

        auto rs = span((uint16_t*)((uint8_t*)&reloc + sizeof(IMAGE_BASE_RELOCATION)), (reloc.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t));

        for (const auto& r : rs) {
            uint16_t offset = r & 0xfff;
            uint16_t type = r >> 12;

            switch (type) {
                case IMAGE_REL_BASED_ABSOLUTE:
                    // nop
                break;

                case IMAGE_REL_BASED_HIGHLOW: {
                    uint32_t* ptr2 = (uint32_t*)((uint8_t*)base + reloc.VirtualAddress + offset);

                    *ptr2 = (uint32_t)(*ptr2 - header_base + (uintptr_t)base);
                    break;
                }

                case IMAGE_REL_BASED_DIR64: {
                    uint64_t* ptr2 = (uint64_t*)((uint8_t*)base + reloc.VirtualAddress + offset);

                    *ptr2 = *ptr2 - header_base + (uintptr_t)base;
                    break;
                }

                default:
                    print("Unsupported relocation type ");
                    print_hex(type);
                    print(".\n");
                    // FIXME - terminate process
                    return;
            }
        }

        dir = dir.subspan(reloc.SizeOfBlock);

        if (dir.size() < sizeof(IMAGE_BASE_RELOCATION))
            break;
    } while (true);

    // FIXME - do we need to update the base in the header?
}

extern "C" __declspec(dllexport)
void mn_start_process(void* img_base, void* ntdll_base) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)img_base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)img_base + dos_header.e_lfanew);

    resolve_relocations(ntdll_base);
    resolve_relocations(img_base);

    // FIXME - load DLLs (recursively) (with relocations)

    resolve_imports(img_base);

    uint32_t entry_point;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        entry_point = nt_header.OptionalHeader64.AddressOfEntryPoint;
    else
        entry_point = nt_header.OptionalHeader32.AddressOfEntryPoint;

    __asm __volatile(
        "jmp %0"
        :
        : "a" ((uint8_t*)img_base + entry_point)
    );
}
