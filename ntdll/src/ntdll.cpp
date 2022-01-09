#include "ntdll.h"
#include "../../kernel/src/pe.h"
#include <optional>

using namespace std;

// FIXME - syscall header file

using NTSTATUS = int32_t;
using USHORT = uint16_t;
using PWSTR = char16_t*;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

extern "C" NTSTATUS NtDisplayString(PUNICODE_STRING String);

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}

static void print(u16string_view s) {
    // FIXME - print to stderr instead

    UNICODE_STRING us;
    us.Length = us.MaximumLength = (uint16_t)(s.size() * sizeof(char16_t));
    us.Buffer = (PWSTR)s.data();

    NtDisplayString(&us);
}

static void print(string_view s) {
    UNICODE_STRING us;
    char16_t buf[255];

    // FIXME - print to stderr instead

    utf8_to_utf16(buf, s);

    us.Length = us.MaximumLength = (uint16_t)(min(sizeof(buf), utf8_to_utf16_len(s)) * sizeof(char16_t));
    us.Buffer = (PWSTR)buf;

    NtDisplayString(&us);
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
        print(u"invalid export directory\n");
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
        print(u"FIXME - load DLLs\n"); // FIXME - die if can't find
        return;
    }

    void* dll_base = (void*)0x400000; // FIXME

    while (*lookup_table) {
        auto v = *lookup_table;

        if (v & ((T)1 << ((sizeof(T) * 8) - 1))) {
            print(u"FIXME - imports by ordinal\n"); // FIXME
            lookup_table++;
            thunks++;
            continue;
        }

        // FIXME - don't ignore hint
        auto func = string_view((char*)base + (v & 0x7fffffff) + sizeof(char16_t));

        auto addr = find_export(dll_base, func);
        if (!addr) {
            print(u"Could not resolve ");
            print(func);
            print(u" in ");
            print(name);
            print(u".\n");
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

extern "C" __declspec(dllexport)
void mn_start_process(void* img_base) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)img_base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)img_base + dos_header.e_lfanew);

    // FIXME - load DLLs (recursively)
    // FIXME - relocations

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
