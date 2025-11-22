module;

#include <variant>
#include <shared_mutex>
#include <span>
#include <optional>
#include <string_view>
#include <functional>
#include <mutex>
#include <utility>
#include "section.h"
#include "process.h"

export module section;

import format;
import print;
import mem;
import config;
import bsod;
import rangelock;
import unicode;
import thread;
import pe;
import file;
import crt;

using namespace std;

extern "C++"
class image_section_cache {
public:
    image_section_cache(section& sect) : sect(sect) { }

    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages);

    page_cache<image_section_cache> cache{*this};

private:
    section& sect;
};

static_assert(cacheable<image_section_cache>);

export nt_status create_section(const optional<reference_wrapper<file>>& file, bool image,
                                size_t size, uint32_t page_protection, object_ref& ref) {
    auto sptr = new section;
    if (!sptr) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto& s = *sptr;

    s.page_protection = page_protection;

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

            s.image_cache = new image_section_cache(s);
            if (!s.image_cache) {
                print("new failed\n");
                s.release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            auto status = s.load_image();
            if (!success(status)) {
                print("load_image returned {}\n", status);
                s.release();
                return status;
            }
        } else {
            s.type = section_type::file;

            {
                shared_lock lock(s.ino->get().inode_lock);
                s.num_pages = bytes_to_pages(s.ino->get().file_size);
            }

            s.size = s.num_pages << PAGE_SHIFT;

            if (s.num_pages == 0) {
                print("create_section: file was empty\n");
                s.release();
                return nt_status::INVALID_PARAMETER;
            }
        }
    } else {
        s.type = section_type::anonymous;
        s.num_pages = bytes_to_pages(size);

        if (s.num_pages == 0) {
            print("anonymous section must specify a size\n");
            s.release();
            return nt_status::INVALID_PARAMETER;
        }

        s.size = s.num_pages << PAGE_SHIFT;

        s.anon_cache = new anon_section_cache;
        if (!s.anon_cache) {
            print("new failed\n");
            s.release();
            return nt_status::INSUFFICIENT_RESOURCES;
        }
    }

    ref = object_ref{s};

    return nt_status::SUCCESS;
}

extern "C++"
section::~section() {
    if (ino)
        ino->get().release();

    if (pe_sections)
        free(pe_sections);

    if (image_cache)
        delete image_cache;

    if (anon_cache)
        delete anon_cache;

    if (custom_cache)
        delete custom_cache;

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

    uint64_t pinned_pages;

    auto status = f.cache.read_into_cache_and_pin(page_start, page_end - page_start + 1,
                                                  pinned_pages);

    if (!success(status) || pinned_pages < page_end - page_start + 1) {
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

extern "C++"
nt_status section::load_image() {
    if (!ino) {
        print("inode was NULL\n");
        return nt_status::INVALID_PARAMETER;
    }

    auto& f = ino->get();
    uint32_t section_alignment;
    page_cache_entry* src_p;
    uint64_t read_pages;

    auto status = f.cache.read_into_cache_and_pin(0, 1, read_pages);

    if (!success(status)) {
        print("section::load_image: read_into_cache_and_pin returned {}\n", status);
        return status;
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

    auto& cache = image_cache->cache;

    status = cache.pin(0, num_pages, nullptr);
    if (!success(status)) {
        print("page_cache::pin returned {}\n", status);
        f.cache.unpin(0, 1);
        return status;
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
        } else if (name == ".debug_line") {
            dwinfo.debug_line.offset = sect.VirtualAddress;
            dwinfo.debug_line.size = sect.VirtualSize;
        } else if (name == ".debug_line_str") {
            dwinfo.debug_line_str.offset = sect.VirtualAddress;
            dwinfo.debug_line_str.size = sect.VirtualSize;
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

        if (n - offset > dir.size()) {
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

extern "C++"
uint64_t section::find_export(string_view name) {
    if (type != section_type::image) {
        print("Not an image.\n");
        return 0;
    }

    IMAGE_DATA_DIRECTORY exp_dir;
    page_cache_entry* p;

    auto& cache = image_cache->cache;
    uint64_t read_pages;

    auto status = cache.read_into_cache_and_pin(0, 1, read_pages);
    if (!success(status)) {
        print("read_into_cache_and_pin returned {}\n", status);
        return 0; // FIXME - return error
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

    uint64_t pinned_pages;

    status = cache.read_into_cache_and_pin(exp_dir_start >> PAGE_SHIFT,
                                           exp_dir_pages, pinned_pages);

    if (!success(status)) {
        print("section::find_export: read_into_cache_and_pin returned {}\n",
              status);
        free_va(va, exp_dir_pages);
        return 0; // FIXME - return error
    } else if (pinned_pages != exp_dir_pages) {
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

extern "C"
NTSTATUS NtCreateSection(HANDLE* section_handle, ACCESS_MASK, const OBJECT_ATTRIBUTES*,
                         const LARGE_INTEGER* maximum_size, uint32_t section_page_protection,
                         uint32_t allocation_attributes, HANDLE file_handle) {
    auto t = get_current_thread();
    auto p = t->proc;
    uint64_t size;
    optional<reference_wrapper<file>> f;
    uint32_t needed_access;

    // FIXME - desired_access
    // FIXME - object_attributes
    // FIXME - allocation_attributes that aren't SEC_IMAGE

    switch (section_page_protection) {
        case PAGE_READONLY:
        case PAGE_EXECUTE_READ:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_WRITECOPY:
            needed_access = FILE_READ_DATA;
            break;

        case PAGE_READWRITE:
        case PAGE_EXECUTE_READWRITE:
            if (allocation_attributes & SEC_IMAGE)
                needed_access = FILE_READ_DATA;
            else
                needed_access = FILE_READ_DATA | FILE_WRITE_DATA;
            break;

        case PAGE_EXECUTE:
        case PAGE_NOACCESS:
            needed_access = 0;
            break;

        default:
            return nt_status::INVALID_PAGE_PROTECTION;
    }

    if (maximum_size && maximum_size->QuadPart < 0)
        return nt_status::INVALID_PARAMETER;

    size = maximum_size ? (uint64_t)maximum_size->QuadPart : 0;

    if (file_handle) {
        ACCESS_MASK file_access_mask;

        auto objrw = deref_handle(file_handle, file_access_mask);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        if ((file_access_mask & needed_access) != needed_access)
            return nt_status::ACCESS_DENIED;

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
                                 section_page_protection, secth);

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

extern "C"
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

        p = (process*)t->proc; // wtf GCC 15?

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

    if (win32_protect & PAGE_NOCACHE) {
        print("NtMapViewOfSection: FIXME - PAGE_NOCACHE\n");
        return nt_status::INVALID_PARAMETER;
    } else if (win32_protect & PAGE_WRITECOMBINE) {
        print("NtMapViewOfSection: FIXME - PAGE_WRITECOMBINE\n");
        return nt_status::INVALID_PARAMETER;
    }

    enum mapping_type protect;
    bool cow = false;

    switch (win32_protect) {
        case PAGE_EXECUTE_READ:
            protect = mapping_type::read_execute;
            cow = false;
            break;

        case PAGE_EXECUTE_READWRITE:
            protect = mapping_type::read_write_execute;
            cow = false;
            break;

        case PAGE_EXECUTE_WRITECOPY:
            protect = mapping_type::read_write_execute;
            cow = true;
            break;

        case PAGE_READONLY:
            protect = mapping_type::read_only;
            cow = false;
            break;

        case PAGE_READWRITE:
            protect = mapping_type::read_write;
            cow = false;
            break;

        case PAGE_WRITECOPY:
            protect = mapping_type::read_write;
            cow = true;
            break;

        default:
            print("NtMapViewOfSection: unrecognized protection flags {:x}\n", win32_protect);
            return nt_status::INVALID_PARAMETER;
    }

    if (sect.type != section_type::image) {
        if (protect == mapping_type::read_write_execute ||
            protect == mapping_type::read_execute) {
            switch (sect.page_protection) {
                case PAGE_EXECUTE_READ:
                case PAGE_EXECUTE_WRITECOPY:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE:
                    break;

                default:
                    return nt_status::ACCESS_DENIED;
            }
        }

        if (!cow && (protect == mapping_type::read_write_execute ||
                     protect == mapping_type::read_write)) {
            switch (sect.page_protection) {
                case PAGE_READWRITE:
                case PAGE_EXECUTE_READWRITE:
                    break;

                default:
                    return nt_status::ACCESS_DENIED;
            }
        }
    }

    optional<int64_t> off;

    if (section_offset)
        off = section_offset->QuadPart;

    return p->map_view_of_section(sect, base_address, zero_bits, commit_size,
                                  off, *view_size, inherit_disposition, allocation_type,
                                  protect, cow);
}

extern "C"
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

extern "C++"
NTSTATUS section::query_info(enum SECTION_INFORMATION_CLASS section_information_class, span<uint8_t> buf,
                             size_t* return_length) {
    if (custom_cache) {
        auto status = custom_cache->query_info(section_information_class, buf,
                                               return_length);
        if (status != nt_status::INVALID_INFO_CLASS)
            return status;
    }

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

extern "C"
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

export uint64_t get_image_size(void* image) {
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

extern "C++"
cache_variant section::get_cache() {
    switch (type) {
        case section_type::image:
            return &image_cache->cache;

        case section_type::anonymous:
            if (custom_cache)
                return &custom_cache->cache;
            else
                return &anon_cache->cache;

        default:
            return &ino->get().cache;
    }
}

extern "C++"
nt_status image_section_cache::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                               uint64_t& read_pages) {
    auto& f = sect.ino->get();
    auto header_pages = bytes_to_pages(sect.size_of_headers);
    page_cache_entry* src_p;
    page_cache_entry* dest_p;
    auto orig_num_pages = num_pages;

    // FIXME - if something fails, should return what we've read so far rather than 0

    if (page_num < header_pages) {
        uint64_t cache_read_pages;

        auto status = f.cache.read_into_cache_and_pin(page_num, min(header_pages - page_num, num_pages),
                                                      cache_read_pages);
        if (!success(status)) {
            print("image_read_into_cache: read_into_cache_and_pin returned {}\n",
                  status);
            return status;
        }

        for (unsigned int i = 0; i < cache_read_pages; i++) {
            size_t to_copy;

            if (page_num + i == header_pages - 1 && sect.size_of_headers % PAGE_SIZE)
                to_copy = sect.size_of_headers % PAGE_SIZE;
            else
                to_copy = PAGE_SIZE;

            {
                lock_guard lg(cache.cache_lock);

                if (i == 0)
                    dest_p = cache.find_page_cache_entry(page_num);
                else
                    dest_p = cache.next_page_cache_entry(*dest_p);
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

                    f.cache.unpin(page_num, cache_read_pages);

                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                memcpy(get_phys_address(*dest_p), get_phys_address(*src_p),
                       to_copy);
                memset((uint8_t*)get_phys_address(*dest_p) + to_copy, 0,
                       PAGE_SIZE - to_copy);
            }
        }

        f.cache.unpin(page_num, cache_read_pages);

        page_num += cache_read_pages;
        num_pages -= cache_read_pages;

        if (num_pages == 0) {
            read_pages = cache_read_pages;
            return nt_status::SUCCESS;
        }
    }

    unsigned int cur_sect = 0;
    bool first = true;

    while (num_pages > 0) {
        {
            lock_guard lg(cache.cache_lock);

            if (first)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);
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
                uint64_t pinned_pages;

                auto status = f.cache.read_into_cache_and_pin(file_start_page, num_file_pages,
                                                              pinned_pages);
                if (!success(status))
                    return status;
                else if (pinned_pages < num_file_pages) {
                    if (pinned_pages != 0)
                        f.cache.unpin(file_start_page, pinned_pages);

                    return nt_status::INTERNAL_ERROR;
                }

                auto to_copy = min((size_t)(s.VirtualAddress + min(s.VirtualSize, s.SizeOfRawData) - (page_num << PAGE_SHIFT)), PAGE_SIZE);

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
                    return nt_status::INSUFFICIENT_RESOURCES;
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

    read_pages = orig_num_pages;
    return nt_status::SUCCESS;
}

extern "C++"
nt_status anon_section_cache::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                              uint64_t& read_pages) {
    // infinite anonymous sections are used for process VM

    auto status = cache.pin(page_num, num_pages, nullptr);
    if (!success(status)) {
        print("page_cache::pin returned {}\n", status);
        return status;
    }

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(page_num);
            else
                p = cache.next_page_cache_entry(*p);
        }

        if (p->pfn)
            continue;

        increase_page_refcount(zero_pfn);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = zero_pfn;
#pragma GCC diagnostic pop
    }

    cache.unpin(page_num, num_pages);

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

extern "C++"
nt_status section::flush(uint64_t page_num, uint64_t num_pages) {
    return visit([&](auto* ptr) {
        if constexpr (cacheable_flushable<typename remove_pointer_t<decltype(ptr)>::dev_type>)
            return ptr->flush(page_num, num_pages, false);
        else
            return nt_status::SUCCESS;
    }, get_cache());
}

extern "C"
NTSTATUS NtOpenSection(HANDLE* section_handle, ACCESS_MASK desired_access,
                       const OBJECT_ATTRIBUTES* object_attributes) {
    auto t = get_current_thread();
    auto p = t->proc;
    nt_status status;

    if (!p)
        return nt_status::INVALID_PARAMETER;

    // FIXME - check permissions

    if (!object_attributes || !section_handle)
        return nt_status::INVALID_PARAMETER;

    if (!object_attributes->ObjectName || !object_attributes->ObjectName->Buffer)
        return nt_status::INVALID_PARAMETER;

    auto u16name = u16string_view(object_attributes->ObjectName->Buffer, object_attributes->ObjectName->Length / sizeof(char16_t));

    if (u16name.empty())
        return nt_status::INVALID_PARAMETER;

    auto namelen = utf16_to_utf8_len(u16name);

    if (namelen == 0)
        return nt_status::INVALID_PARAMETER;

    auto name = (char*)malloc(namelen);
    if (!name)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(name, namelen), u16name);

    for (unsigned int i = 0; i < namelen; i++) {
        if (name[i] == '\\')
            name[i] = '/';
    }

    static const string_view dev_prefix = "/dev/";
    object_ref obj_ref;

    if (namelen < dev_prefix.size() || string_view(name, dev_prefix.size()) != dev_prefix) {
        free(name);
        return nt_status::OBJECT_NAME_NOT_FOUND;
    }

    status = open_object(string_view(name + dev_prefix.size(), namelen - dev_prefix.size()),
                         obj_ref);
    if (!success(status)) {
        free(name);
        return status;
    }

    free(name);

    if (obj_ref.get().type != object_type::section)
        return nt_status::OBJECT_TYPE_MISMATCH;

    handle_t h;

    status = p->add_handle(obj_ref.get(), desired_access, true, h);
    if (!success(status))
        return status;

    *section_handle = h;

    return nt_status::SUCCESS;
}

extern "C++"
nt_status section::ioctl(uint32_t code, span<const uint8_t> inbuf,
                         span<uint8_t> outbuf, uintptr_t& written) {
    return visit([&](auto* ptr) {
        if constexpr (has_ioctls<typename remove_pointer_t<decltype(ptr)>::dev_type>)
            return ptr->dev.ioctl(code, inbuf, outbuf, written);
        else
            return nt_status::NOT_SUPPORTED;
    }, get_cache());
}
