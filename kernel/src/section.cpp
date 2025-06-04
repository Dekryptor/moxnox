#include "kernel.h"
#include "handle.h"
#include "spinlock.h"
import rbtree;
import event;
import pipe;
import timer;
import objects;
#include "inode.h"
import volume;
#include "handle.h"
#include "section.h"
#include "file.h"
#include "thread.h"
#include "process.h"
#include <pe.h>
#include <utility>

import format;
import print;
import mem;
import config;
import pe;
import bsod;
import rangelock;

using namespace std;

nt_status create_section(const optional<reference_wrapper<file>>& file, bool image,
                         size_t size, object_ref& ref) {
    auto sptr = new section;
    if (!sptr) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto& s = *sptr;

    if (file) {
        if (file->get().ino.type != object_type::inode) {
            print("file pointed to object other than inode\n");
            s.release();
            return nt_status::INVALID_PARAMETER;
        }

        if (file->get().name) {
            s.name = (char*)malloc(file->get().namelen);
            if (!s.name) {
                print("malloc failed\n");
                s.release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            s.namelen = file->get().namelen;
            memcpy(s.name, file->get().name, s.namelen);
        }

        s.ino = static_cast<inode&>(file->get().ino);
        s.ino->get().acquire();

        if (image) {
            s.type = section_type::image;
            // FIXME - also work with ELFs

            auto status = s.load_image();
            if (!success(status)) {
                print("load_image returned {}\n", status);
                s.release();
                return status;
            }
        } else
            s.type = section_type::file;
    } else {
        s.type = section_type::anonymous;
        s.num_pages = bytes_to_pages(size);

        if (s.num_pages == 0) {
            print("anonymous section must specify a size\n");
            s.release();
            return nt_status::INVALID_PARAMETER;
        }

        s.size = s.num_pages << PAGE_SHIFT;

        // FIXME - make sure allocate_page doesn't steal what we've reserved
    }

    ref = object_ref{s};

    return nt_status::SUCCESS;
}

section::~section() {
    if (ino)
        ino->get().release();

    if (pe_sections)
        free(pe_sections);

    free(name);
}

static span<char> get_section_name(const IMAGE_SECTION_HEADER& s, const IMAGE_FILE_HEADER& file_header,
                                   inode& f, span<char> buf) {
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

    size_t copy_start = string_table + num;
    size_t copy_end = copy_start + buf.size() - 1;
    auto page_start = (uint64_t)(copy_start >> PAGE_SHIFT);
    auto page_end = (uint64_t)(copy_end >> PAGE_SHIFT);

    auto pinned_pages = (uint32_t)f.cache.read_into_cache_and_pin(page_start, page_end - page_start + 1);

    if (pinned_pages < page_end - page_start + 1) {
        f.cache.unpin(page_start, pinned_pages);
        return format(buf, "{}", name);
    }

    f.cache.copy_into_buffer_and_unpin(page_start, pinned_pages, span((uint8_t*)buf.data(), buf.size()),
                                       copy_start % PAGE_SIZE);

    size_t end = buf.size();
    for (unsigned int i = 0; i < end; i++) {
        if (buf[i] == 0) {
            end = i;
            break;
        }
    }

    return buf.subspan(end);
}

nt_status section::load_image() {
    if (!ino) {
        print("inode was NULL\n");
        return nt_status::INVALID_PARAMETER;
    }

    auto& f = ino->get();
    uint32_t section_alignment;
    page_cache_entry* src_p;

    if (f.cache.read_into_cache_and_pin(0, 1) == 0) {
        print("section::load_image: read_into_cache_and_pin failed\n");
        return nt_status::INTERNAL_ERROR;
    }

    {
        lock_guard lock(f.cache.cache_lock);
        src_p = f.cache.find_page_cache_entry(0);
    }

    if (!src_p) {
        print("cache::find_page_cache_entry failed\n");
        f.cache.unpin(0, 1);
        return nt_status::INTERNAL_ERROR;
    }

    auto& dos_header = *(IMAGE_DOS_HEADER*)get_phys_address(*src_p);

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
        print("invalid image header\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    // FIXME - make sure we don't read past end of pagae

    if (dos_header.e_lfanew < sizeof(IMAGE_DOS_HEADER) || dos_header.e_lfanew > PAGE_SIZE - sizeof(IMAGE_NT_HEADERS)) {
        print("invalid e_lfanew value\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)get_phys_address(*src_p) + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE) {
        print("incorrect PE signature\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    characteristics = nt_header.FileHeader.Characteristics;
    machine = nt_header.FileHeader.Machine;

    if (nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        print("unrecognized optional header signature\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    // FIXME - check checksum

    // FIXME - check Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
            print("SizeOfOptionalHeader was smaller than expected\n");
            f.cache.unpin(0, 1);
            return nt_status::INVALID_IMAGE_FORMAT;
        }

        size = nt_header.OptionalHeader64.SizeOfImage;
        size_of_headers = nt_header.OptionalHeader64.SizeOfHeaders;
        preferred_base = nt_header.OptionalHeader64.ImageBase;
        stack_size = nt_header.OptionalHeader64.SizeOfStackReserve;
        entry_point = nt_header.OptionalHeader64.AddressOfEntryPoint;
        section_alignment = nt_header.OptionalHeader64.SectionAlignment;
    } else {
        if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)) {
            print("SizeOfOptionalHeader was smaller than expected\n");
            f.cache.unpin(0, 1);
            return nt_status::INVALID_IMAGE_FORMAT;
        }

        size = nt_header.OptionalHeader32.SizeOfImage;
        size_of_headers = nt_header.OptionalHeader32.SizeOfHeaders;
        preferred_base = nt_header.OptionalHeader32.ImageBase;
        stack_size = nt_header.OptionalHeader32.SizeOfStackReserve;
        entry_point = nt_header.OptionalHeader32.AddressOfEntryPoint;
        section_alignment = nt_header.OptionalHeader32.SectionAlignment;
    }

    if (size == 0) {
        print("size of image was 0\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    if (size_of_headers > size) {
        print("SizeOfHeaders was more than SizeOfImage\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    if (nt_header.FileHeader.NumberOfSections == 0) {
        print("NumberOfSections was 0\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    // FIXME - does this happen in the real world?
    if (section_alignment % PAGE_SIZE) {
        print("SectionAlignment was not a multiple of PAGE_SIZE\n");
        f.cache.unpin(0, 1);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    num_pages = bytes_to_pages(size);
    size = num_pages << PAGE_SHIFT;

    if (!cache.pin(0, num_pages, nullptr)) {
        print("page_cache::pin failed\n");
        f.cache.unpin(0, 1);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    f.cache.unpin(0, 1);

    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    memset(&dwinfo, 0, sizeof(dwinfo));

    for (unsigned int i = 0; i < sections.size(); i++) {
        auto& sect = sections[i];
        auto section_size = max(sect.VirtualSize, sect.SizeOfRawData);
        char buf[50];
        string_view name;

        auto sp = span(buf, sizeof(buf));
        sp = get_section_name(sect, nt_header.FileHeader, f, sp);

        name = string_view(buf, (size_t)(sp.data() - buf));

        if (name == ".debug_info") {
            dwinfo.debug_info.offset = sect.VirtualAddress;
            dwinfo.debug_info.size = sect.VirtualSize;
        } else if (name == ".debug_abbrev") {
            dwinfo.debug_abbrev.offset = sect.VirtualAddress;
            dwinfo.debug_abbrev.size = sect.VirtualSize;
        } else if (name == ".debug_rnglists") {
            dwinfo.debug_rnglists.offset = sect.VirtualAddress;
            dwinfo.debug_rnglists.size = sect.VirtualSize;
        } else if (name == ".debug_str") {
            dwinfo.debug_str.offset = sect.VirtualAddress;
            dwinfo.debug_str.size = sect.VirtualSize;
        } else if (name == ".debug_addr") {
            dwinfo.debug_addr.offset = sect.VirtualAddress;
            dwinfo.debug_addr.size = sect.VirtualSize;
        } else if (name == ".debug_str_offsets") {
            dwinfo.debug_str_offsets.offset = sect.VirtualAddress;
            dwinfo.debug_str_offsets.size = sect.VirtualSize;
        }

        if (sect.VirtualAddress % section_alignment) {
            print("section aligned incorrectly\n");
            cache.unpin(0, num_pages);
            return nt_status::INVALID_IMAGE_FORMAT;
        }

        if (i != 0 && sections[i].VirtualAddress < sections[i - 1].VirtualAddress + sections[i - 1].VirtualSize) {
            print("section overlap\n");
            cache.unpin(0, num_pages);
            return nt_status::INVALID_IMAGE_FORMAT;
        }

        if (i != 0 && sections[i].VirtualAddress < sections[i - 1].VirtualAddress) {
            print("sections out of order\n");
            cache.unpin(0, num_pages);
            return nt_status::INVALID_IMAGE_FORMAT;
        }

        if (sect.VirtualAddress + section_size > size) {
            print("section overflow\n");
            cache.unpin(0, num_pages);
            return nt_status::INVALID_IMAGE_FORMAT;
        }

        // FIXME - make sure sect.PointerToRawData isn't out of bounds of file
    }

    // don't confuse malloc
    if (sizeof(IMAGE_SECTION_HEADER) * num_pe_sections > PAGE_SIZE) {
        print("too many sections\n");
        cache.unpin(0, num_pages);
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    num_pe_sections = nt_header.FileHeader.NumberOfSections;
    pe_sections = (IMAGE_SECTION_HEADER*)malloc(sizeof(IMAGE_SECTION_HEADER) * num_pe_sections);
    if (!pe_sections) {
        print("malloc failed\n");
        cache.unpin(0, num_pages);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(pe_sections, sections.data(), num_pe_sections * sizeof(IMAGE_SECTION_HEADER));

    return nt_status::SUCCESS;
}

static uint64_t search_exports(span<uint8_t> dir, uint64_t offset, string_view name) {
    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)dir.data();

    if (ied.NumberOfNames == 0)
        return 0;

    auto names = span((uint32_t*)(dir.data() + ied.AddressOfNames - offset), ied.NumberOfNames);

    if ((uint8_t*)names.data() + names.size_bytes() > dir.data() + dir.size()) {
        print("invalid export directory\n");
        return 0;
    }

    auto name_ordinals = span((uint16_t*)(dir.data() + ied.AddressOfNameOrdinals - offset), ied.NumberOfNames);

    if ((uint8_t*)name_ordinals.data() + name_ordinals.size_bytes() > dir.data() + dir.size()) {
        print("invalid export directory\n");
        return 0;
    }

    auto funcs = span((uint32_t*)(dir.data() + ied.AddressOfFunctions - offset), ied.NumberOfFunctions);

    if ((uint8_t*)funcs.data() + funcs.size_bytes() > dir.data() + dir.size()) {
        print("invalid export directory\n");
        return 0;
    }

    optional<uint16_t> ord;

    for (unsigned int i = 0; i < names.size(); i++) {
        auto n = names[i];

        if (dir.data() + n - offset > dir.data() + dir.size()) {
            print("invalid export directory\n");
            return 0;
        }

        auto sv = string_view((char*)dir.data() + n - offset, dir.size() + offset - n);

        auto nulpos = sv.find_first_of((char)0);

        if (nulpos != string_view::npos)
            sv = string_view(sv.data(), nulpos);

        if (sv == name) {
            ord = name_ordinals[i];
            break;
        }
    }

    if (!ord)
        return 0;

    if (*ord >= ied.NumberOfFunctions) {
        print("Function ordinal out of range.\n");
        return 0;
    }

    return funcs[*ord];
}

uint64_t section::find_export(string_view name) {
    if (type != section_type::image) {
        print("Not an image.\n");
        return 0;
    }

    IMAGE_DATA_DIRECTORY exp_dir;
    page_cache_entry* p;

    if (cache.read_into_cache_and_pin(0, 1) == 0) {
        print("read_into_cache_and_pin failed\n");
        return 0;
    }

    {
        lock_guard lg(cache.cache_lock);

        p = cache.find_page_cache_entry(0);
    }

    {
        auto va = get_phys_address(*p);

        bool found = false;

        const auto& dos_header = *(IMAGE_DOS_HEADER*)va;
        const auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)va + dos_header.e_lfanew);

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

        if (!found) {
            print("image has no export directory\n");
            cache.unpin(0, 1);
            return 0;
        }
    }

    cache.unpin(0, 1);

    if (exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) {
        print("invalid export directory\n");
        return 0;
    }

    uint64_t exp_dir_start = exp_dir.VirtualAddress & ~(PAGE_SIZE - 1);
    uint64_t exp_dir_end = bytes_to_pages(exp_dir.VirtualAddress + exp_dir.Size) << PAGE_SHIFT;
    auto exp_dir_pages = bytes_to_pages(exp_dir_end - exp_dir_start);

    // FIXME - avoid doing mapping here (will need to rewrite search_exports)

    auto va = allocate_va(exp_dir_pages);
    if (!va) {
        print("allocate_va failed\n");
        return 0;
    }

    auto pinned_pages = cache.read_into_cache_and_pin(exp_dir_start >> PAGE_SHIFT,
                                                      exp_dir_pages);

    if (pinned_pages != exp_dir_pages) {
        print("section::find_export: read_into_cache_and_pin returned {}, expected {}\n",
              pinned_pages, exp_dir_pages);
        cache.unpin(exp_dir_start >> PAGE_SHIFT, pinned_pages);
        free_va(va, exp_dir_pages);
        return 0;
    }

    for (unsigned int i = 0; i < exp_dir_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(exp_dir_start >> PAGE_SHIFT);
            else
                p = cache.next_page_cache_entry(*p);
        }

        if (!map_page(p->pfn << PAGE_SHIFT, va + (i << PAGE_SHIFT), mapping_type::read_only)) {
            print("map_page failed\n");
            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }
            free_va(va, exp_dir_pages);
            cache.unpin(exp_dir_start >> PAGE_SHIFT, exp_dir_pages);
            return 0;
        }
    }

    auto ret = search_exports(span((uint8_t*)va + exp_dir.VirtualAddress - exp_dir_start, exp_dir.Size),
                              exp_dir.VirtualAddress, name);

    for (unsigned int i = 0; i < exp_dir_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, exp_dir_pages);
    cache.unpin(exp_dir_start >> PAGE_SHIFT, exp_dir_pages);

    return ret;
}

NTSTATUS NtCreateSection(HANDLE* section_handle, ACCESS_MASK, const OBJECT_ATTRIBUTES*,
                         const LARGE_INTEGER* maximum_size, uint32_t,
                         uint32_t allocation_attributes, HANDLE file_handle) {
    auto t = get_current_thread();
    auto p = t->proc;
    uint64_t size;
    optional<reference_wrapper<file>> f;

    // FIXME - desired_access
    // FIXME - object_attributes
    // FIXME - section_page_protection
    // FIXME - allocation_attributes that aren't SEC_IMAGE

    if (maximum_size && maximum_size->QuadPart < 0)
        return nt_status::INVALID_PARAMETER;

    size = maximum_size ? (uint64_t)maximum_size->QuadPart : 0;

    if (file_handle) {
        auto objrw = deref_handle(file_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        if (objrw.get().type == object_type::file) {
            auto& f2 = *static_cast<file*>(&objrw.get());

            if (f2.ino.type == object_type::inode)
                f = f2;
        }

        if (!f)
            return nt_status::INVALID_HANDLE;

        f->get().acquire();
    }

    object_ref secth;

    auto status = create_section(f, allocation_attributes & SEC_IMAGE, size,
                                 secth);

    if (f)
        f->get().release();

    if (!success(status))
        return status;

    handle_t h;

    status = p->add_handle(secth.get(), 0, true, h); // FIXME - access mask
    if (!success(status))
        return status;

    *section_handle = h;

    return nt_status::SUCCESS;
}

NTSTATUS NtMapViewOfSection(HANDLE section_handle, HANDLE process_handle, void** base_address,
                            uintptr_t zero_bits, size_t commit_size, LARGE_INTEGER* section_offset,
                            size_t* view_size, enum SECTION_INHERIT inherit_disposition,
                            uint32_t allocation_type, uint32_t win32_protect) {
    object_ref proc;
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;

        proc = object_ref{*p};
        p->acquire();
    } else {
        proc = deref_handle(process_handle);
        if (!proc)
            return nt_status::INVALID_HANDLE;

        auto& obj = proc.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        p = static_cast<process*>(&obj);
    }

    auto objrw = deref_handle(section_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::section)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& sect = static_cast<section&>(obj);

    return p->map_view_of_section(sect, base_address, zero_bits, commit_size, section_offset,
                                  *view_size, inherit_disposition, allocation_type,
                                  win32_protect);
}

NTSTATUS NtUnmapViewOfSection(HANDLE process_handle, void* base_address) {
    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        return t->proc->unmap_view_of_section(base_address);
    } else {
        auto objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw.get();

        if (obj.type != object_type::process)
            return nt_status::OBJECT_TYPE_MISMATCH;

        // FIXME - check access rights

        auto& p = *static_cast<process*>(&obj);

        return p.unmap_view_of_section(base_address);
    }
}

NTSTATUS section::query_info(enum SECTION_INFORMATION_CLASS section_information_class, span<uint8_t> buf,
                             size_t* return_length) {
    switch (section_information_class) {
        case SECTION_INFORMATION_CLASS::ImageInformation: {
            if (buf.size() < sizeof(SECTION_IMAGE_INFORMATION))
                return nt_status::INFO_LENGTH_MISMATCH;

            if (type != section_type::image)
                return nt_status::INVALID_PARAMETER;

            auto& sii = *(SECTION_IMAGE_INFORMATION*)buf.data();

            sii.TransferAddress = (void*)(preferred_base + entry_point);
            sii.ZeroBits = 0; // FIXME
            sii.MaximumStackSize = 0; // FIXME
            sii.CommittedStackSize = 0; // FIXME
            sii.SubSystemType = 0; // FIXME
            sii.SubSystemMinorVersion = 0; // FIXME
            sii.SubSystemMajorVersion = 0; // FIXME
            sii.GpValue = 0; // FIXME
            sii.ImageCharacteristics = characteristics;
            sii.DllCharacteristics = 0; // FIXME
            sii.Machine = machine;
            sii.ImageContainsCode = false; // FIXME
            sii.ImageFlags = 0; // FIXME
            sii.LoaderFlags = 0; // FIXME
            sii.ImageFileSize = 0; // FIXME
            sii.CheckSum = 0; // FIXME

            if (return_length)
                *return_length = sizeof(SECTION_IMAGE_INFORMATION);

            return nt_status::SUCCESS;
        }

        default:
            print("section::query_info: unhandled info class {}\n",
                  (unsigned int)section_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

NTSTATUS NtQuerySection(HANDLE section_handle, enum SECTION_INFORMATION_CLASS section_information_class,
                        void* section_information, size_t section_information_length, size_t* return_length) {
    auto objrw = deref_handle(section_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::section)
        return nt_status::OBJECT_TYPE_MISMATCH;

    // FIXME - check access rights

    auto& s = *static_cast<section*>(&obj);

    return s.query_info(section_information_class, span((uint8_t*)section_information, section_information_length),
                        return_length);
}

uint64_t get_image_size(void* image) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)image;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    if (dos_header.e_lfanew < sizeof(IMAGE_DOS_HEADER) || dos_header.e_lfanew > PAGE_SIZE - sizeof(IMAGE_NT_HEADERS))
        return 0;

    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)image + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE)
        return 0;

    if (nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return 0;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
            return 0;

        return nt_header.OptionalHeader64.SizeOfImage;
    } else if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32))
            return 0;

        return nt_header.OptionalHeader32.SizeOfImage;
    } else
        return 0;
}

size_t image_read_into_cache(section& sect, uint64_t page_num, uint64_t num_pages) {
    auto& f = sect.ino->get();
    auto header_pages = bytes_to_pages(sect.size_of_headers);
    page_cache_entry* src_p;
    page_cache_entry* dest_p;
    auto orig_num_pages = num_pages;

    // FIXME - if something fails, should return what we've read so far rather than 0

    if (page_num < header_pages) {
        auto read_pages = f.cache.read_into_cache_and_pin(page_num, min(header_pages - page_num, num_pages));
        if (read_pages == 0) {
            print("image_read_into_cache: read_into_cache_and_pin failed\n");
            return 0;
        }

        for (unsigned int i = 0; i < read_pages; i++) {
            size_t to_copy;

            if (page_num + i == header_pages - 1 && sect.size_of_headers % PAGE_SIZE)
                to_copy = sect.size_of_headers % PAGE_SIZE;
            else
                to_copy = PAGE_SIZE;

            {
                lock_guard lg(sect.cache.cache_lock);

                if (i == 0)
                    dest_p = sect.cache.find_page_cache_entry(page_num);
                else
                    dest_p = sect.cache.next_page_cache_entry(*dest_p);
            }

            {
                lock_guard lg(f.cache.cache_lock);

                if (i == 0)
                    src_p = f.cache.find_page_cache_entry(page_num);
                else
                    src_p = f.cache.next_page_cache_entry(*src_p);
            }

            if (to_copy == PAGE_SIZE) {
                rangelock_req rlr(f.cache.rl, page_num + i, 1);
                increase_page_refcount(src_p->pfn);
                dest_p->pfn = src_p->pfn;
            } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!dest_p->pfn) {
                    print("allocate_folio failed\n");

                    f.cache.unpin(page_num, read_pages);
                    // FIXME - set error to nt_status::INSUFFICIENT_RESOURCES
                    return 0;
                }

                memcpy(get_phys_address(*dest_p), get_phys_address(*src_p),
                       to_copy);
                memset((uint8_t*)get_phys_address(*dest_p) + to_copy, 0,
                       PAGE_SIZE - to_copy);
            }
        }

        f.cache.unpin(page_num, read_pages);

        page_num += read_pages;
        num_pages -= read_pages;

        if (num_pages == 0)
            return read_pages;
    }

    unsigned int cur_sect = 0;
    bool first = true;

    while (num_pages > 0) {
        {
            lock_guard lg(sect.cache.cache_lock);

            if (first)
                dest_p = sect.cache.find_page_cache_entry(page_num);
            else
                dest_p = sect.cache.next_page_cache_entry(*dest_p);
        }

        first = false;

        if (((page_num + 1) << PAGE_SHIFT) - 1 < sect.pe_sections[cur_sect].VirtualAddress) {
            increase_page_refcount(zero_pfn);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            dest_p->pfn = zero_pfn;
#pragma GCC diagnostic pop

            page_num++;
            num_pages--;
            continue;
        }

        while (true) {
            auto& s = sect.pe_sections[cur_sect];
            auto section_size = (uint64_t)max(s.VirtualSize, s.SizeOfRawData);

            if (section_size & (PAGE_SIZE - 1)) {
                section_size &= ~(PAGE_SIZE - 1);
                section_size += PAGE_SIZE;
            }

            if (page_num << PAGE_SHIFT >= s.VirtualAddress && ((page_num + 1) << PAGE_SHIFT) <= s.VirtualAddress + section_size) {
                if (page_num << PAGE_SHIFT >= s.VirtualAddress + s.SizeOfRawData) {
                    increase_page_refcount(zero_pfn);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    dest_p->pfn = zero_pfn;
#pragma GCC diagnostic pop
                    break;
                }

                uint64_t file_start = s.PointerToRawData + (page_num << PAGE_SHIFT) - s.VirtualAddress;
                uint64_t file_end = s.PointerToRawData + ((page_num + 1) << PAGE_SHIFT) - 1 - s.VirtualAddress;
                auto file_start_page = file_start >> PAGE_SHIFT;
                auto file_end_page = file_end >> PAGE_SHIFT;
                auto num_file_pages = file_end_page - file_start_page + 1;
                uint64_t offset = s.PointerToRawData & (PAGE_SIZE - 1);

                auto pinned_pages = f.cache.read_into_cache_and_pin(file_start_page, num_file_pages);
                if (pinned_pages < num_file_pages) {
                    if (pinned_pages != 0)
                        f.cache.unpin(file_start_page, pinned_pages);

                    // FIXME - set error to nt_status::INTERNAL_ERROR
                    return 0;
                }

                auto to_copy = min((size_t)(s.VirtualAddress + s.VirtualSize - (page_num << PAGE_SHIFT)), PAGE_SIZE);

                // if aligned nicely, reflink rather than copying
                if (offset == 0 && to_copy == PAGE_SIZE) {
                    {
                        lock_guard lock(f.cache.cache_lock);
                        src_p = f.cache.find_page_cache_entry(file_start_page);
                    }

                    uint64_t pfn;

                    {
                        rangelock_req rlr(f.cache.rl, file_start_page, 1);
                        increase_page_refcount(src_p->pfn);
                        pfn = src_p->pfn;
                    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    dest_p->pfn = pfn;
#pragma GCC diagnostic pop
                    break;
                }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!dest_p->pfn) {
                    print("allocate_folio failed\n");
                    f.cache.unpin(file_start_page, num_file_pages);
                    // FIXME - set error to nt_status::INSUFFICIENT_RESOURCES
                    return 0;
                }

                auto sp = span((uint8_t*)get_phys_address(*dest_p), to_copy);

                f.cache.copy_into_buffer(file_start_page, num_file_pages,
                                         sp, offset);
                f.cache.unpin(file_start_page, num_file_pages);

                if (sp.size() < PAGE_SIZE) {
                    memset((uint8_t*)get_phys_address(*dest_p) + sp.size(), 0,
                           PAGE_SIZE - sp.size());
                }

                break;
            }

            if (cur_sect >= sect.num_pe_sections - 1) {
                increase_page_refcount(zero_pfn);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = zero_pfn;
#pragma GCC diagnostic pop
                break;
            }

            cur_sect++;
        }

        page_num++;
        num_pages--;
    }

    return orig_num_pages;
}

size_t file_read_into_cache(section& /*sect*/, uint64_t /*page_num*/,
                            uint64_t /*num_pages*/) {
    print("file_read_into_cache: FIXME - file mappings\n"); // FIXME
    return 0;
}

size_t anon_read_into_cache(section& sect, uint64_t page_num, uint64_t num_pages) {
    if (page_num >= sect.num_pages)
        return 0;

    if (page_num + num_pages > sect.num_pages)
        num_pages = sect.num_pages - page_num;

    if (!sect.cache.pin(page_num, num_pages, nullptr)) {
        print("page_cache::pin failed\n");
        return 0;
    }

    page_cache_entry* p;
    uint64_t read_pages = num_pages;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(sect.cache.cache_lock);

            if (i == 0)
                p = sect.cache.find_page_cache_entry(page_num);
            else
                p = sect.cache.next_page_cache_entry(*p);
        }

        if (p->pfn)
            continue;

        increase_page_refcount(zero_pfn);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = zero_pfn;
#pragma GCC diagnostic pop
    }

    sect.cache.unpin(page_num, num_pages);

    return read_pages;
}

size_t section::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    switch (type) {
        case section_type::image:
            return image_read_into_cache(*this, page_num, num_pages);

        case section_type::file:
            return file_read_into_cache(*this, page_num, num_pages);

        case section_type::anonymous:
            return anon_read_into_cache(*this, page_num, num_pages);

        default:
            unreachable();
    }
}

size_t section::flush_cache(uint64_t page_num, uint64_t num_pages) {
    print("FIXME: section::flush_cache({}, {})\n", page_num, num_pages);
    // FIXME

    return 0;
}
