#include "ntdll_private.h"
#include "list.h"
#include "../../kernel/src/pe.h"
#include <optional>
#include <nt.h>

using namespace std;

extern uint8_t __image_base__;

static_assert(sizeof(PEB_LDR_DATA) == 0x58);
static_assert(sizeof(LDR_DATA_TABLE_ENTRY) == 0x88);

static PEB_LDR_DATA ldr_data; // zero-initialized

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}

void print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

void print_hex(uint64_t num, unsigned int padding) {
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

static void toggle_protections(void* base, bool enable) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);
    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    for (const auto& sect : sections) {
        auto section_size = min(sect.VirtualSize, sect.SizeOfRawData);

        if (section_size == 0 || sect.PointerToRawData == 0)
            continue;

        void* addr = (uint8_t*)base + sect.VirtualAddress;
        size_t size = section_size;
        uint32_t new_prot, old_prot;

        if (enable) {
            if (sect.Characteristics & IMAGE_SCN_MEM_WRITE && !(sect.Characteristics & IMAGE_SCN_MEM_EXECUTE))
                new_prot = PAGE_READWRITE;
            else if (sect.Characteristics & IMAGE_SCN_MEM_WRITE && sect.Characteristics & IMAGE_SCN_MEM_EXECUTE)
                new_prot = PAGE_EXECUTE_READWRITE;
            else if (!(sect.Characteristics & IMAGE_SCN_MEM_WRITE) && !(sect.Characteristics & IMAGE_SCN_MEM_EXECUTE))
                new_prot = PAGE_READONLY;
            else if (!(sect.Characteristics & IMAGE_SCN_MEM_WRITE) && sect.Characteristics & IMAGE_SCN_MEM_EXECUTE)
                new_prot = PAGE_EXECUTE_READ;
        } else
            new_prot = sect.Characteristics & IMAGE_SCN_MEM_EXECUTE ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

        auto ret = NtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, new_prot, &old_prot);

        if (!success(ret)) {
            print("NtProtectVirtualMemory returned ");
            print_hex((uint32_t)ret);
            print(".\n");
            NtTerminateProcess(NtCurrentProcess(), ret);
        }
    }
}

static void add_ldr_entry(void* base, u16string_view full_name, u16string_view base_name) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto size = sizeof(LDR_DATA_TABLE_ENTRY) + ((full_name.size() + 1) * sizeof(char16_t)) + ((base_name.size() + 1) * sizeof(char16_t));
    auto ptr = RtlAllocateHeap((void*)peb.ProcessHeap, HEAP_ZERO_MEMORY, size);

    if (!ptr) {
        print("RtlAllocateHeap failed.\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);
    }

    auto& ent = *(LDR_DATA_TABLE_ENTRY*)ptr;

    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    ent.DllBase = base;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        ent.EntryPoint = (uint8_t*)base + nt_header.OptionalHeader64.AddressOfEntryPoint;
        ent.SizeOfImage = nt_header.OptionalHeader64.SizeOfImage;
    } else {
        ent.EntryPoint = (uint8_t*)base + nt_header.OptionalHeader32.AddressOfEntryPoint;
        ent.SizeOfImage = nt_header.OptionalHeader32.SizeOfImage;
    }

    ent.FullDllName.Length = (uint16_t)(full_name.size() * sizeof(char16_t));
    ent.FullDllName.MaximumLength = ent.FullDllName.Length + sizeof(char16_t);
    ent.FullDllName.Buffer = (char16_t*)((uint8_t*)ptr + sizeof(LDR_DATA_TABLE_ENTRY));

    ent.BaseDllName.Length = (uint16_t)(base_name.size() * sizeof(char16_t));
    ent.BaseDllName.MaximumLength = ent.BaseDllName.Length + sizeof(char16_t);
    ent.BaseDllName.Buffer = (char16_t*)((uint8_t*)ent.FullDllName.Buffer + ent.FullDllName.MaximumLength);

    // FIXME - Flags
    ent.LoadCount = 1;
    // FIXME - HashLinks

    memcpy(ent.FullDllName.Buffer, full_name.data(), ent.FullDllName.Length);
    ent.FullDllName.Buffer[full_name.size()] = 0;

    memcpy(ent.BaseDllName.Buffer, base_name.data(), ent.BaseDllName.Length);
    ent.BaseDllName.Buffer[base_name.size()] = 0;

    InsertTailList(&ldr_data.InLoadOrderModuleList, &ent.InLoadOrderLinks);

    {
        auto le = ldr_data.InMemoryOrderModuleList.Flink;
        bool done = false;

        while (le != &ldr_data.InMemoryOrderModuleList) {
            auto ent2 = CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            if (ent2->DllBase > ent.DllBase) {
                InsertHeadList(le->Blink, &ent.InMemoryOrderLinks);
                done = true;
                break;
            }

            le = le->Flink;
        }

        if (!done)
            InsertTailList(&ldr_data.InMemoryOrderModuleList, &ent.InMemoryOrderLinks);
    }
}

static void init_ldr_data(void* img_base, void* ntdll_base) {
    ldr_data.Length = sizeof(ldr_data);
    ldr_data.Initialized = true;
    InitializeListHead(&ldr_data.InLoadOrderModuleList);
    InitializeListHead(&ldr_data.InMemoryOrderModuleList);
    InitializeListHead(&ldr_data.InInitializationOrderModuleList);

    add_ldr_entry(ntdll_base, u"/ntdll.dll", u"ntdll.dll"); // FIXME - name and path
    add_ldr_entry(img_base, u"/init.exe", u"init.exe"); // FIXME - name and path
}

extern "C" __declspec(dllexport)
void mn_start_process(void* img_base, void* ntdll_base) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto& dos_header = *(IMAGE_DOS_HEADER*)img_base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)img_base + dos_header.e_lfanew);

    auto heap = RtlCreateHeap(HEAP_GROWABLE, nullptr, 0, 0, nullptr, nullptr);

    if (!heap) {
        print("RtlCreateHeap failed\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::NO_MEMORY);
    }

    peb.ProcessHeap = (HANDLE)heap;
    peb.Ldr = &ldr_data;

    init_ldr_data(peb.ImageBaseAddress, ntdll_base);

    toggle_protections(ntdll_base, false);
    toggle_protections(img_base, false);

    resolve_relocations(ntdll_base);
    resolve_relocations(img_base);

    // FIXME - load DLLs (recursively) (with relocations)

    resolve_imports(img_base);

    toggle_protections(ntdll_base, true);
    toggle_protections(img_base, true);

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
