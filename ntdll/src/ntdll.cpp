#include "ntdll_private.h"
#include "list.h"
#include <pe.h>
#include <optional>
#include <nt.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

import print;

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

static void add_ldr_entry(void* base, u16string_view full_name, u16string_view base_name, bool no_dll_main) {
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

    ent.LoadCount = 1;
    // FIXME - HashLinks

    if (no_dll_main)
        ent.ProcessAttachCalled = 1;

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

static void init_ldr_data(void* ntdll_base) {
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

    add_ldr_entry(ntdll_base, u"\\ntdll.dll", u"ntdll.dll", true); // FIXME - name and path
    add_ldr_entry(peb.ImageBaseAddress, image_path, base_name, true);
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

    status = NtCreateFile(&h, 0, &oa, &iosb, nullptr, 0, 0, FILE_OPEN, 0, nullptr, 0);

    if (!success(status)) {
        free(us.Buffer);
        return false;
    }

    status = NtCreateSection(&sect, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE, h);

    NtClose(h);

    if (!success(status)) {
        free(us.Buffer);
        return false;
    }

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr, &size,
                                SECTION_INHERIT::ViewUnmap, 0, PAGE_READONLY);

    NtClose(sect);

    if (!success(status)) {
        free(us.Buffer);
        return false;
    }

    auto& dos_header = *(IMAGE_DOS_HEADER*)addr;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)addr + dos_header.e_lfanew);

    if (nt_header.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        free(us.Buffer);
        return false;
    }

    if (!(nt_header.FileHeader.Characteristics & IMAGE_FILE_DLL)) {
        NtUnmapViewOfSection(NtCurrentProcess(), addr);
        free(us.Buffer);
        return false;
    }

    add_ldr_entry(addr, u16string_view(us.Buffer, us.Length / sizeof(char16_t)), name, false);

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

extern "C"
void mn_start_thread(void* ntdll_base, CONTEXT* context) {
    if (!initialized) {
        auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

        auto heap = RtlCreateHeap(HEAP_GROWABLE, nullptr, 0, 0, nullptr, nullptr);

        if (!heap) {
            print("RtlCreateHeap failed\n");
            NtTerminateProcess(NtCurrentProcess(), nt_status::NO_MEMORY);
        }

        peb.ProcessHeap = (HANDLE)heap;
        peb.Ldr = &ldr_data;

        init_ldr_data(ntdll_base);

        load_dlls();

        toggle_protections(false);

        resolve_relocations();

        resolve_imports();

        toggle_protections(true);

        call_dll_main();

        initialized = true;
    }

    // FIXME - call DllMain for modules if starting another thread

    NtContinue(context, false);
}
