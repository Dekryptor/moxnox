#include "ntdll_private.h"
#include "list.h"
#include <pe.h>
#include <optional>
#include <stdlib.h>
#include <string.h>

using namespace std;

import print;
import format;

extern uint8_t __image_base__;

static_assert(sizeof(PEB_LDR_DATA) == 0x58);
static_assert(sizeof(LDR_DATA_TABLE_ENTRY) == 0x88);

using DLL_ENTRY_PROC = bool (*)(void*, uint32_t, void*);

static PEB_LDR_DATA ldr_data; // zero-initialized
static bool initialized = false;

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}

// work round GCC 15 ICE (FIXME)
static NTSTATUS _LdrGetProcedureAddress(void* dll_handle, const ANSI_STRING* procedure_name,
                                        uint32_t /*procedure_number*/, void** procedure_address) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)dll_handle;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)dll_handle + dos_header.e_lfanew);
    uintptr_t dir_addr;

    // FIXME - searching by procedure number

    if (!procedure_name)
        return nt_status::INVALID_PARAMETER;

    string_view name(procedure_name->Buffer, procedure_name->Length);

    if (name.empty())
        return nt_status::INVALID_PARAMETER;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
            print("image has no export directory\n");
            return nt_status::INTERNAL_ERROR;
        }

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader64.DataDirectory);
    } else {
        if (nt_header.OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
            print("image has no export directory\n");
            return nt_status::INTERNAL_ERROR;
        }

        dir_addr = dos_header.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader32.DataDirectory);
    }

    dir_addr += IMAGE_DIRECTORY_ENTRY_EXPORT * sizeof(IMAGE_DATA_DIRECTORY);

    auto& data_dir = *(IMAGE_DATA_DIRECTORY*)((uint8_t*)dll_handle + dir_addr);

    if (data_dir.VirtualAddress == 0 || data_dir.Size == 0) {
        print("image has no export directory\n");
        return nt_status::INTERNAL_ERROR;
    }

    // search export directory

    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)((uint8_t*)dll_handle + data_dir.VirtualAddress);

    if (ied.NumberOfNames == 0) {
        print("image has no exports\n");
        return nt_status::INTERNAL_ERROR;
    }

    auto names = span((uint32_t*)((uint8_t*)&ied + ied.AddressOfNames - data_dir.VirtualAddress),
                      ied.NumberOfNames);

    if ((uint8_t*)names.data() + names.size_bytes() > (uint8_t*)&ied + data_dir.Size) {
        print("invalid export directory\n");
        return nt_status::INTERNAL_ERROR;
    }

    auto name_ordinals = span((uint16_t*)((uint8_t*)&ied + ied.AddressOfNameOrdinals - data_dir.VirtualAddress),
                              ied.NumberOfNames);

    if ((uint8_t*)name_ordinals.data() + name_ordinals.size_bytes() > (uint8_t*)&ied + data_dir.Size) {
        print("invalid export directory\n");
        return nt_status::INTERNAL_ERROR;
    }

    auto funcs = span((uint32_t*)((uint8_t*)&ied + ied.AddressOfFunctions - data_dir.VirtualAddress),
                      ied.NumberOfFunctions);

    if ((uint8_t*)funcs.data() + funcs.size_bytes() > (uint8_t*)&ied + data_dir.Size) {
        print("invalid export directory\n");
        return nt_status::INTERNAL_ERROR;
    }

    optional<uint16_t> ord;

    for (unsigned int i = 0; i < names.size(); i++) {
        auto n = names[i];

        if (n - data_dir.VirtualAddress > data_dir.Size) {
            print("invalid export directory\n");
            return nt_status::INTERNAL_ERROR;
        }

        auto sv = string_view((char*)(uint8_t*)&ied + n - data_dir.VirtualAddress,
                              data_dir.Size + data_dir.VirtualAddress - n);

        auto nulpos = sv.find_first_of((char)0);

        if (nulpos != string_view::npos)
            sv = string_view(sv.data(), nulpos);

        if (sv == name) {
            ord = name_ordinals[i];
            break;
        }
    }

    if (!ord) {
        print("could not find export {}\n", name);
        return nt_status::INTERNAL_ERROR;
    }

    if (*ord >= ied.NumberOfFunctions) {
        print("function ordinal out of range\n");
        return nt_status::INTERNAL_ERROR;
    }

    *procedure_address = (uint8_t*)dll_handle + funcs[*ord];

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS LdrGetProcedureAddress(void* dll_handle, const ANSI_STRING* procedure_name,
                                uint32_t procedure_number, void** procedure_address) {
    return _LdrGetProcedureAddress(dll_handle, procedure_name,
                                   procedure_number, procedure_address);
}

template<typename T>
static void resolve_imports_dll(void* base, void* dll_base, string_view name, T* lookup_table, T* thunks) {
    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    if (!*lookup_table)
        return;

    const auto& dos_header = *(IMAGE_DOS_HEADER*)dll_base;
    const auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)dll_base + dos_header.e_lfanew);

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

    if (!found || exp_dir.VirtualAddress == 0) {
        print("no export directory in {}\n", name);
        NtTerminateProcess(NtCurrentProcess(), nt_status::PROCEDURE_NOT_FOUND);
    }

    if (exp_dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) {
        print("invalid export directory in {}\n", name);
        NtTerminateProcess(NtCurrentProcess(), nt_status::PROCEDURE_NOT_FOUND);
    }

    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)((uint8_t*)dll_base + exp_dir.VirtualAddress);

    if (ied.NumberOfFunctions == 0) {
        print("no exports in {}\n", name);
        NtTerminateProcess(NtCurrentProcess(), nt_status::PROCEDURE_NOT_FOUND);
    }

    auto names = span((uint32_t*)((uint8_t*)dll_base + ied.AddressOfNames), ied.NumberOfNames);

    auto name_ordinals = span((uint16_t*)((uint8_t*)dll_base + ied.AddressOfNameOrdinals), ied.NumberOfNames);

    auto funcs = span((uint32_t*)((uint8_t*)dll_base + ied.AddressOfFunctions), ied.NumberOfFunctions);

    while (*lookup_table) {
        auto v = *lookup_table;

        if (v & ((T)1 << ((sizeof(T) * 8) - 1))) {
            auto ord = (uint16_t)(v & 0xffff);

            if (ord < ied.Base || ord >= ied.Base + ied.NumberOfFunctions || funcs[ord - ied.Base] == 0) {
                print("Ordinal {} in {} not found.\n", ord, name);
                NtTerminateProcess(NtCurrentProcess(), nt_status::ORDINAL_NOT_FOUND);
            }

            auto addr = funcs[ord - ied.Base];

            *thunks = (T)((uintptr_t)dll_base + addr);

            lookup_table++;
            thunks++;

            continue;
        }

        // FIXME - don't ignore hint
        auto func = string_view((char*)base + (v & 0x7fffffff) + sizeof(char16_t));

        optional<uint16_t> ord;

        for (unsigned int i = 0; i < names.size(); i++) {
            auto n = names[i];

            auto sv = string_view((char*)dll_base + n);

            if (sv == func) {
                ord = name_ordinals[i];
                break;
            }
        }

        if (!ord || funcs[*ord] == 0) {
            print("Could not resolve {} in {}.\n", func, name);
            NtTerminateProcess(NtCurrentProcess(), nt_status::PROCEDURE_NOT_FOUND);
        }

        auto addr = funcs[*ord];

        *thunks = (T)((uintptr_t)dll_base + addr);

        lookup_table++;
        thunks++;
    }
}

static void resolve_imports_img(void* base) {
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

        auto name = string_view((char*)base + iid.Name); // FIXME - to UTF-16
        LDR_DATA_TABLE_ENTRY* ent = nullptr;

        auto u16len = utf8_to_utf16_len(name);

        if (u16len == 0) {
            print("DLL name is empty.\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_NOT_FOUND);
        }

        auto u16buf = (char16_t*)malloc(u16len * sizeof(char16_t));

        if (!u16buf)
            NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);

        utf8_to_utf16(span(u16buf, u16len), name);

        auto u16sv = u16string_view_ci(u16buf, u16len);

        {
            auto le = ldr_data.InLoadOrderModuleList.Flink;

            while (le != &ldr_data.InLoadOrderModuleList) {
                auto& ent2 = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
                auto base_name = u16string_view_ci(ent2.BaseDllName.Buffer, ent2.BaseDllName.Length / sizeof(char16_t));

                if (base_name == u16sv) {
                    ent = &ent2;
                    break;
                }

                le = le->Flink;
            }
        }

        free(u16buf);

        if (!ent) {
            print("{} not loaded.\n", name);
            NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_NOT_FOUND);
        }

        if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            resolve_imports_dll(base, ent->DllBase, name,
                                (uint64_t*)((uint8_t*)base + iid.Characteristics),
                                (uint64_t*)((uint8_t*)base + iid.FirstThunk));
        } else {
            resolve_imports_dll(base, ent->DllBase, name,
                                (uint32_t*)((uint8_t*)base + iid.Characteristics),
                                (uint32_t*)((uint8_t*)base + iid.FirstThunk));
        }
    }
}

static void resolve_imports() {
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        resolve_imports_img(ent.DllBase);

        le = le->Flink;
    }
}

static void resolve_relocations_img(void* base) {
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
                    print("Unsupported relocation type {:x}.\n", type);
                    NtTerminateProcess(NtCurrentProcess(), nt_status::ILLEGAL_DLL_RELOCATION);
                    return;
            }
        }

        dir = dir.subspan(reloc.SizeOfBlock);

        if (dir.size() < sizeof(IMAGE_BASE_RELOCATION))
            break;
    } while (true);

    // FIXME - do we need to update the base in the header?
}

static void resolve_relocations() {
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        resolve_relocations_img(ent.DllBase);

        le = le->Flink;
    }
}

static void toggle_protections_img(void* base, bool enable) {
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
        uint32_t new_prot = 0, old_prot;

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
            print("NtProtectVirtualMemory returned {}.\n", ret);
            NtTerminateProcess(NtCurrentProcess(), ret);
        }
    }
}

static void toggle_protections(bool enable) {
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        toggle_protections_img(ent.DllBase, enable);

        le = le->Flink;
    }
}

static ldr_entry& add_ldr_entry(void* base, u16string_view full_name, u16string_view base_name, bool no_dll_main) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto size = sizeof(ldr_entry) + ((full_name.size() + 1) * sizeof(char16_t)) + ((base_name.size() + 1) * sizeof(char16_t));
    auto ptr = RtlAllocateHeap((void*)peb.ProcessHeap, HEAP_ZERO_MEMORY, size);

    if (!ptr) {
        print("RtlAllocateHeap failed.\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);
    }

    auto& ent = *(ldr_entry*)ptr;
    auto& ldr = ent.ldr;

    auto& dos_header = *(IMAGE_DOS_HEADER*)base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)base + dos_header.e_lfanew);

    ldr.DllBase = base;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        ldr.EntryPoint = (uint8_t*)base + nt_header.OptionalHeader64.AddressOfEntryPoint;
        ldr.SizeOfImage = nt_header.OptionalHeader64.SizeOfImage;
    } else {
        ldr.EntryPoint = (uint8_t*)base + nt_header.OptionalHeader32.AddressOfEntryPoint;
        ldr.SizeOfImage = nt_header.OptionalHeader32.SizeOfImage;
    }

    ldr.FullDllName.Length = (uint16_t)(full_name.size() * sizeof(char16_t));
    ldr.FullDllName.MaximumLength = ldr.FullDllName.Length + sizeof(char16_t);
    ldr.FullDllName.Buffer = (char16_t*)((uint8_t*)ptr + sizeof(ldr_entry));

    ldr.BaseDllName.Length = (uint16_t)(base_name.size() * sizeof(char16_t));
    ldr.BaseDllName.MaximumLength = ldr.BaseDllName.Length + sizeof(char16_t);
    ldr.BaseDllName.Buffer = (char16_t*)((uint8_t*)ldr.FullDllName.Buffer + ldr.FullDllName.MaximumLength);

    ldr.LoadCount = 1;
    // FIXME - HashLinks

    if (no_dll_main)
        ldr.ProcessAttachCalled = 1;

    memcpy(ldr.FullDllName.Buffer, full_name.data(), ldr.FullDllName.Length);
    ldr.FullDllName.Buffer[full_name.size()] = 0;

    memcpy(ldr.BaseDllName.Buffer, base_name.data(), ldr.BaseDllName.Length);
    ldr.BaseDllName.Buffer[base_name.size()] = 0;

    InsertTailList(&ldr_data.InLoadOrderModuleList, &ldr.InLoadOrderLinks);

    {
        auto le = ldr_data.InMemoryOrderModuleList.Flink;
        bool done = false;

        while (le != &ldr_data.InMemoryOrderModuleList) {
            auto ent2 = CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

            if (ent2->DllBase > ldr.DllBase) {
                InsertHeadList(le->Blink, &ldr.InMemoryOrderLinks);
                done = true;
                break;
            }

            le = le->Flink;
        }

        if (!done)
            InsertTailList(&ldr_data.InMemoryOrderModuleList, &ldr.InMemoryOrderLinks);
    }

    return ent;
}

static void init_ldr_data(mn_image_info& ntdll_info, mn_image_info& img_info) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto& proc_params = *peb.ProcessParameters;

    ldr_data.Length = sizeof(ldr_data);
    ldr_data.Initialized = true;
    InitializeListHead(&ldr_data.InLoadOrderModuleList);
    InitializeListHead(&ldr_data.InMemoryOrderModuleList);
    InitializeListHead(&ldr_data.InInitializationOrderModuleList);

    u16string_view image_path(proc_params.ImagePathName.Buffer,
                              proc_params.ImagePathName.Length / sizeof(char16_t));
    u16string_view base_name = image_path;

    if (auto pos = base_name.find_last_of('\\'); pos != u16string_view::npos)
        base_name = u16string_view(base_name.data() + pos + 1, base_name.size() - pos - 1);

    if (auto pos = base_name.find_last_of('/'); pos != u16string_view::npos)
        base_name = u16string_view(base_name.data() + pos + 1, base_name.size() - pos - 1);

    auto& ntdll = add_ldr_entry(ntdll_info.base, u"\\ntdll.dll", u"ntdll.dll", true); // FIXME - name and path

    ntdll.offset_debug_info = ntdll_info.offset_debug_info;
    ntdll.size_debug_info = ntdll_info.size_debug_info;
    ntdll.offset_debug_abbrev = ntdll_info.offset_debug_abbrev;
    ntdll.size_debug_abbrev = ntdll_info.size_debug_abbrev;
    ntdll.offset_debug_rnglists = ntdll_info.offset_debug_rnglists;
    ntdll.size_debug_rnglists = ntdll_info.size_debug_rnglists;
    ntdll.offset_debug_str = ntdll_info.offset_debug_str;
    ntdll.size_debug_str = ntdll_info.size_debug_str;
    ntdll.offset_debug_addr = ntdll_info.offset_debug_addr;
    ntdll.size_debug_addr = ntdll_info.size_debug_addr;
    ntdll.offset_debug_str_offsets = ntdll_info.offset_debug_str_offsets;
    ntdll.size_debug_str_offsets = ntdll_info.size_debug_str_offsets;
    ntdll.offset_debug_line = ntdll_info.offset_debug_line;
    ntdll.size_debug_line = ntdll_info.size_debug_line;
    ntdll.offset_debug_line_str = ntdll_info.offset_debug_line_str;
    ntdll.size_debug_line_str = ntdll_info.size_debug_line_str;

    auto& img = add_ldr_entry(img_info.base, image_path, base_name, true);

    img.offset_debug_info = img_info.offset_debug_info;
    img.size_debug_info = img_info.size_debug_info;
    img.offset_debug_abbrev = img_info.offset_debug_abbrev;
    img.size_debug_abbrev = img_info.size_debug_abbrev;
    img.offset_debug_rnglists = img_info.offset_debug_rnglists;
    img.size_debug_rnglists = img_info.size_debug_rnglists;
    img.offset_debug_str = img_info.offset_debug_str;
    img.size_debug_str = img_info.size_debug_str;
    img.offset_debug_addr = img_info.offset_debug_addr;
    img.size_debug_addr = img_info.size_debug_addr;
    img.offset_debug_str_offsets = img_info.offset_debug_str_offsets;
    img.size_debug_str_offsets = img_info.size_debug_str_offsets;
    img.offset_debug_line = img_info.offset_debug_line;
    img.size_debug_line = img_info.size_debug_line;
    img.offset_debug_line_str = img_info.offset_debug_line_str;
    img.size_debug_line_str = img_info.size_debug_line_str;
}

static span<char> get_section_name(const IMAGE_SECTION_HEADER& s, const IMAGE_FILE_HEADER& file_header,
                                   HANDLE h, span<char> buf) {
    auto name = string_view(s.Name, sizeof(s.Name));

    while (!name.empty() && name.back() == 0) {
        name.remove_suffix(1);
    }

    if (name.empty() || name[0] != '/' || file_header.PointerToSymbolTable == 0)
        return format(buf, "{}", name);

    // decode long section names

    unsigned int num = 0;
    uint32_t string_table = file_header.PointerToSymbolTable + (file_header.NumberOfSymbols * 18);

    for (auto c : string_view(name.data() + 1, name.size() - 1)) {
        num *= 10;

        if (c < '0' || c > '9')
            return format(buf, "{}", name);

        num += (unsigned int)(c - '0');
    }

    LARGE_INTEGER off;
    IO_STATUS_BLOCK iosb;

    off.QuadPart = string_table + num;

    auto status = NtReadFile(h, 0, nullptr, nullptr, &iosb, buf.data(),
                             (uint32_t)buf.size(), &off, nullptr);
    if (!success(status))
        return format(buf, "{}", name);

    if (iosb.Information < buf.size())
        buf = span(buf.data(), iosb.Information);

    size_t end = buf.size();
    for (unsigned int i = 0; i < end; i++) {
        if (buf[i] == 0) {
            end = i;
            break;
        }
    }

    return buf.subspan(end);
}

static bool try_load_dll(u16string_view dir, u16string_view name) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h, sect;
    IO_STATUS_BLOCK iosb;
    void* addr = nullptr;
    size_t size = 0;

    us.Length = us.MaximumLength = (uint16_t)((dir.size() + name.size()) * sizeof(char16_t));
    us.Buffer = (char16_t*)malloc(us.Length);

    if (!us.Buffer)
        NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);

    memcpy(us.Buffer, dir.data(), dir.size() * sizeof(char16_t));
    memcpy(us.Buffer + dir.size(), name.data(), name.size() * sizeof(char16_t));

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, 0, nullptr, 0);

    if (!success(status)) {
        free(us.Buffer);
        return false;
    }

    status = NtCreateSection(&sect, SECTION_MAP_READ, nullptr, nullptr,
                             PAGE_READONLY, SEC_IMAGE, h);

    if (!success(status)) {
        NtClose(h);
        free(us.Buffer);
        return false;
    }

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr, &size,
                                SECTION_INHERIT::ViewUnmap, 0, PAGE_READONLY);

    NtClose(sect);

    if (!success(status)) {
        NtClose(h);
        free(us.Buffer);
        return false;
    }

    auto& dos_header = *(IMAGE_DOS_HEADER*)addr;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)addr + dos_header.e_lfanew);

#ifdef __aarch64__
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_ARM64;
#else
    static const uint16_t this_arch = IMAGE_FILE_MACHINE_AMD64;
#endif

    if (nt_header.FileHeader.Machine != this_arch) {
        NtClose(h);
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        free(us.Buffer);
        return false;
    }

    if (!(nt_header.FileHeader.Characteristics & IMAGE_FILE_DLL)) {
        NtClose(h);
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        free(us.Buffer);
        return false;
    }

    auto& ent = add_ldr_entry(addr, u16string_view(us.Buffer, us.Length / sizeof(char16_t)),
                              name, false);

    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    for (const auto& sect : sections) {
        char buf[50];
        span sp(buf, sizeof(buf));

        sp = get_section_name(sect, nt_header.FileHeader, h, sp);

        auto name = string_view(buf, (size_t)(sp.data() - buf));

        if (name == ".debug_info") {
            ent.offset_debug_info = sect.VirtualAddress;
            ent.size_debug_info = sect.VirtualSize;
        } else if (name == ".debug_abbrev") {
            ent.offset_debug_abbrev = sect.VirtualAddress;
            ent.size_debug_abbrev = sect.VirtualSize;
        } else if (name == ".debug_rnglists") {
            ent.offset_debug_rnglists = sect.VirtualAddress;
            ent.size_debug_rnglists = sect.VirtualSize;
        } else if (name == ".debug_str") {
            ent.offset_debug_str = sect.VirtualAddress;
            ent.size_debug_str = sect.VirtualSize;
        } else if (name == ".debug_addr") {
            ent.offset_debug_addr = sect.VirtualAddress;
            ent.size_debug_addr = sect.VirtualSize;
        } else if (name == ".debug_str_offsets") {
            ent.offset_debug_str_offsets = sect.VirtualAddress;
            ent.size_debug_str_offsets = sect.VirtualSize;
        } else if (name == ".debug_line") {
            ent.offset_debug_line = sect.VirtualAddress;
            ent.size_debug_line = sect.VirtualSize;
        } else if (name == ".debug_line_str") {
            ent.offset_debug_line_str = sect.VirtualAddress;
            ent.size_debug_line_str = sect.VirtualSize;
        }
    }

    NtClose(h);
    free(us.Buffer);

    return true;
}

static bool load_dll(string_view name) {
    auto u16len = utf8_to_utf16_len(name);

    if (u16len == 0)
        return false;

    auto u16buf = (char16_t*)malloc(u16len * sizeof(char16_t));

    if (!u16buf)
        NtTerminateProcess(NtCurrentProcess(), nt_status::INSUFFICIENT_RESOURCES);

    utf8_to_utf16(span(u16buf, u16len), name);

    auto u16sv = u16string_view_ci(u16buf, u16len);

    {
        auto le = ldr_data.InLoadOrderModuleList.Flink;

        while (le != &ldr_data.InLoadOrderModuleList) {
            auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
            auto base_name = u16string_view_ci(ent.BaseDllName.Buffer, ent.BaseDllName.Length / sizeof(char16_t));

            if (base_name == u16sv) {
                free(u16buf);
                return true;
            }

            le = le->Flink;
        }
    }

    // FIXME - directories to search (same directory as image, then PATH?)

    if (try_load_dll(u"\\", u16string_view(u16sv.data(), u16sv.size()))) {
        free(u16buf);
        return true;
    }

    free(u16buf);

    return false;
}

static void load_dlls_img(LDR_DATA_TABLE_ENTRY& ent) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.DllBase + dos_header.e_lfanew);

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

    auto iids = span((IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)ent.DllBase + exp_dir.VirtualAddress),
                     exp_dir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR));

    for (auto& iid : iids) {
        if (iid.Characteristics == 0)
            break;

        auto name = string_view((char*)ent.DllBase + iid.Name);

        if (name.empty()) {
            print("DLL with empty name.\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_NOT_FOUND);
        }

        if (!load_dll(name)) {
            print("Could not load {}.\n", name);
            NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_NOT_FOUND);
        }
    }
}

static void load_dlls() {
    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        load_dlls_img(ent);

        le = le->Flink;
    }
}

static void process_attach(LDR_DATA_TABLE_ENTRY& ent) {
    if (ent.LoadInProgress || ent.ProcessAttachCalled)
        return;

    ent.LoadInProgress = 1;

    // FIXME - call process_attach for dependencies

    auto dll_main = (DLL_ENTRY_PROC)ent.EntryPoint;

    // FIXME - calling DLL_PROCESS_DETACH when process dies

    if (!dll_main(ent.DllBase, DLL_PROCESS_ATTACH, (void*)1))
        NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_INIT_FAILED);

    ent.ProcessAttachCalled = 1;
}

static void call_dll_main() {
    // FIXME - send in correct order (bottom of tree to top)

    auto le = ldr_data.InLoadOrderModuleList.Flink;

    while (le != &ldr_data.InLoadOrderModuleList) {
        auto& ent = *CONTAINING_RECORD(le, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        process_attach(ent);

        le = le->Flink;
    }
}

#ifdef __aarch64__
extern "C"
void* memcpy(void* dest, const void* src, size_t n) {
    void* orig_dest = dest;

    while (n >= sizeof(uint64_t)) {
        *(__unaligned uint64_t*)dest = *(__unaligned uint64_t*)src;

        dest = (uint8_t*)dest + sizeof(uint64_t);
        src = (uint8_t*)src + sizeof(uint64_t);

        n -= sizeof(uint64_t);
    }

    while (n >= sizeof(uint32_t)) {
        *(__unaligned uint32_t*)dest = *(__unaligned uint32_t*)src;

        dest = (uint8_t*)dest + sizeof(uint32_t);
        src = (uint8_t*)src + sizeof(uint32_t);

        n -= sizeof(uint32_t);
    }

    while (n >= sizeof(uint16_t)) {
        *(__unaligned uint16_t*)dest = *(__unaligned uint16_t*)src;

        dest = (uint8_t*)dest + sizeof(uint16_t);
        src = (uint8_t*)src + sizeof(uint16_t);

        n -= sizeof(uint16_t);
    }

    while (n >= sizeof(uint8_t)) {
        *(uint8_t*)dest = *(uint8_t*)src;

        dest = (uint8_t*)dest + sizeof(uint8_t);
        src = (uint8_t*)src + sizeof(uint8_t);

        n -= sizeof(uint8_t);
    }

    return orig_dest;
}
#endif

extern "C" FORCE_ALIGN_ARG_POINTER
void mn_start_thread(mn_image_info* ntdll_info, mn_image_info* img_info, CONTEXT* context) {
    if (!initialized) {
        auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

        auto heap = RtlCreateHeap(HEAP_GROWABLE, nullptr, 0, 0, nullptr, nullptr);

        if (!heap) {
            print("RtlCreateHeap failed\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::NO_MEMORY);
        }

        peb.ProcessHeap = (HANDLE)heap;
        peb.Ldr = &ldr_data;

        init_ldr_data(*ntdll_info, *img_info);

        // map kernelbase into every process
        if (!load_dll("kernelbase.dll")) {
            print("Could not load kernelbase.dll\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::DLL_NOT_FOUND);
        }

        load_dlls();

        toggle_protections(false);

        resolve_relocations();

        resolve_imports();

        toggle_protections(true);

        call_dll_main();

        initialized = true;
    }

    // FIXME - call DllMain for modules if starting another thread

#ifdef __aarch64__
    context->X18 = (uintptr_t)NtCurrentTeb();
#endif

    NtContinue(context, false);
}
