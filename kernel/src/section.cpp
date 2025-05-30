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

import format;
import print;
import mem;
import config;
import pe;
import bsod;

using namespace std;

// #define debug_printk printk
// #define debug_print_hex print_hex
#define debug_printk(a)
#define debug_print_hex(...)

nt_status create_section(const optional<reference_wrapper<file>>& file, bool image,
                         size_t size, object_ref& ref) {
    if (file && !image) {
        print("FIXME - non-image sections\n");
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - non-image sections

    auto sptr = new section;
    if (!sptr) {
        debug_printk("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto& s = *sptr;

    if (file) {
        if (file->get().ino.type != object_type::inode) {
            debug_printk("file pointed to object other than inode\n");
            s.release();
            return nt_status::INVALID_PARAMETER;
        }

        if (file->get().name) {
            s.name = (char*)malloc(file->get().namelen);
            if (!s.name) {
                debug_printk("malloc failed\n");
                s.release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            s.namelen = file->get().namelen;
            memcpy(s.name, file->get().name, s.namelen);
        }

        s.ino = static_cast<inode&>(file->get().ino);
        s.ino->get().acquire();

        s.type = section_type::image;
        // FIXME - also work with ELFs

        auto status = s.load_image();
        if (!success(status)) {
            debug_printk("load_image returned ");
            debug_print_hex((uint32_t)status, 8);
            debug_printk("\n");
            s.release();
            return status;
        }
    } else {
        s.type = section_type::anonymous;
        s.num_pages = bytes_to_pages(size);

        if (s.num_pages == 0) {
            debug_printk("anonymous section must specify a size\n");
            s.release();
            return nt_status::INVALID_PARAMETER;
        }

        s.size = s.num_pages << PAGE_SHIFT;

        s.pages_protection = (enum mapping_type*)malloc(sizeof(enum mapping_type) * s.num_pages);
        if (!s.pages_protection) {
            print("malloc failed\n");
            s.release();
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        memset(s.pages_protection, 0, sizeof(enum mapping_type) * s.num_pages);

        // FIXME - protection

        // FIXME - make sure allocate_page doesn't steal what we've reserved
    }

    ref = object_ref{s};

    return nt_status::SUCCESS;
}

section::~section() {
    if (ino)
        ino->get().release();

    free(name);
    free_pa();
}

void section::free_pa() {
    if (pages_protection) {
        free(pages_protection);
        pages_protection = nullptr;
    }
}

class scratch {
public:
    scratch() {
        pa = allocate_page();
        if (!pa) {
            debug_printk("allocate_page failed\n");
            return;
        }

        va = (uintptr_t)phys_address(pa);

        valid = true;
    }

    ~scratch() {
        if (!valid)
            return;

        free_folio(pa);
    }

    bool valid = false;
    uintptr_t pa, va;
};

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
        debug_printk("inode was NULL\n");
        return nt_status::INVALID_PARAMETER;
    }

    auto& f = ino->get();
    uint32_t size_of_headers, header_pages, section_alignment;

    scratch s;

    auto buf = (uint8_t*)s.va;

    uint64_t read_bytes;
    auto status = f.read(span(buf, PAGE_SIZE), 0, read_bytes);
    if (!success(status)) {
        debug_printk("inode read returned ");
        debug_print_hex((uint64_t)status, 8);
        debug_printk("\n");
        return status;
    }

    if (read_bytes < PAGE_SIZE) {
        debug_printk("read less than 1 page\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    auto& dos_header = *(IMAGE_DOS_HEADER*)buf;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
        debug_printk("invalid image header\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    if (dos_header.e_lfanew < sizeof(IMAGE_DOS_HEADER) || dos_header.e_lfanew > PAGE_SIZE - sizeof(IMAGE_NT_HEADERS)) {
        debug_printk("invalid e_lfanew value\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    auto& nt_header = *(IMAGE_NT_HEADERS*)(buf + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE) {
        debug_printk("incorrect PE signature\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    characteristics = nt_header.FileHeader.Characteristics;
    machine = nt_header.FileHeader.Machine;

    if (nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        debug_printk("unrecognized optional header signature\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    // FIXME - check checksum

    // FIXME - check Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
            debug_printk("SizeOfOptionalHeader was smaller than expected\n");
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
            debug_printk("SizeOfOptionalHeader was smaller than expected\n");
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
        debug_printk("size of image was 0\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    if (size_of_headers > size) {
        debug_printk("SizeOfHeaders was more than SizeOfImage\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    if (size_of_headers > PAGE_SIZE) {
        debug_printk("SizeOfHeaders was more than 1 page\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    // FIXME - does this happen in the real world?
    if (section_alignment % PAGE_SIZE) {
        debug_printk("SectionAlignment was not a multiple of PAGE_SIZE\n");
        return nt_status::INVALID_IMAGE_FORMAT;
    }

    num_pages = bytes_to_pages(size);
    size = num_pages << PAGE_SHIFT;

    pages_protection = (enum mapping_type*)malloc(sizeof(enum mapping_type) * num_pages);
    if (!pages_protection) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    // FIXME - allocate zeroed pages?

    if (!cache.pin(0, num_pages, nullptr)) {
        print("page_cache::pin failed\n");
        free(pages_protection);
        pages_protection = nullptr;
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    header_pages = bytes_to_pages(size_of_headers);
    page_cache_entry* p;

    for (unsigned int i = 0; i < header_pages; i++) {
        size_t to_copy;

        if (i == header_pages - 1 && size_of_headers % PAGE_SIZE)
            to_copy = size_of_headers % PAGE_SIZE;
        else
            to_copy = PAGE_SIZE;

        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(0);
            else
                p = cache.next_page_cache_entry(*p);

            if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!p->pfn) {
                    print("allocate_folio failed\n");
                    free(pages_protection);
                    pages_protection = nullptr;
                    return nt_status::INSUFFICIENT_RESOURCES;
                }
            }
        }

        memcpy(get_phys_address(*p), buf + (i << PAGE_SHIFT), to_copy);

        if (to_copy < PAGE_SIZE) {
            memset((uint8_t*)get_phys_address(*p) + to_copy, 0,
                   PAGE_SIZE - to_copy);
        }

        p->status = page_cache_status::clean;
    }

    for (unsigned int i = header_pages; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            p = cache.next_page_cache_entry(*p);

            if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!p->pfn) {
                    print("allocate_folio failed\n");
                    free(pages_protection);
                    pages_protection = nullptr;
                    return nt_status::INSUFFICIENT_RESOURCES;
                }
            }
        }

        memset(get_phys_address(*p), 0, PAGE_SIZE);
        p->status = page_cache_status::clean;
    }

    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    pages_protection[0] = mapping_type::read_only;

    memset(&dwinfo, 0, sizeof(dwinfo));

    for (const auto& sect : sections) {
        auto section_size = max(sect.VirtualSize, sect.SizeOfRawData);
        char buf[50];
        string_view name;

        if (section_size == 0)
            continue;

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

        if (sect.VirtualAddress + section_size > size) {
            debug_printk("section overflow\n");
            cache.unpin(0, num_pages);
            free_pa();
            return nt_status::INVALID_IMAGE_FORMAT;
        }

        if (sect.PointerToRawData != 0) {
            auto file_start_page = sect.PointerToRawData >> PAGE_SHIFT;
            auto file_end_page = (sect.PointerToRawData + section_size - 1) >> PAGE_SHIFT;
            auto file_pages_left = file_end_page - file_start_page + 1;
            uint64_t bytes_left = section_size;
            unsigned int page_num = 0;
            uint64_t offset = sect.PointerToRawData & (PAGE_SIZE - 1);

            if (sect.VirtualAddress % section_alignment) {
                debug_printk("section not aligned to SectionAlignment\n");
                cache.unpin(0, num_pages);
                free_pa();
                return nt_status::INVALID_IMAGE_FORMAT;
            }

            {
                lock_guard lg(cache.cache_lock);

                p = cache.find_page_cache_entry(sect.VirtualAddress >> PAGE_SHIFT);
            }

            assert(p);

            do {
                auto pinned_pages = (uint32_t)f.cache.read_into_cache_and_pin(file_start_page + page_num, file_pages_left);

                if (pinned_pages == 0) {
                    debug_printk("inode::read_into_cache_and_pin did not pin any pages\n");
                    cache.unpin(0, num_pages);
                    free_pa();
                    return nt_status::INTERNAL_ERROR;
                }

                for (unsigned int i = 0; i < pinned_pages; i++) {
                    span<uint8_t> sp;

                    if (page_num != 0 && offset != 0) {
                        sp = span((uint8_t*)get_phys_address(*p) + (PAGE_SIZE - offset),
                                  min(offset, bytes_left));

                        f.cache.copy_into_buffer(file_start_page + page_num, 1, sp, 0);
                        bytes_left -= sp.size();

                        if (bytes_left == 0)
                            break;
                    }

                    if (page_num != 0) {
                        lock_guard lg(cache.cache_lock);

                        p = cache.next_page_cache_entry(*p);
                    }

                    assert(p);

                    sp = span((uint8_t*)get_phys_address(*p), min(PAGE_SIZE - offset, bytes_left));

                    f.cache.copy_into_buffer(file_start_page + page_num, 1, sp, offset);
                    bytes_left -= sp.size();

                    page_num++;
                }

                f.cache.unpin(file_start_page, pinned_pages);

                file_pages_left -= pinned_pages;

                if (file_pages_left == 0)
                    break;
            } while (true);
        }

        auto page_start = sect.VirtualAddress >> PAGE_SHIFT;
        auto page_end = (sect.VirtualAddress + section_size - 1) >> PAGE_SHIFT;

        auto prot = characteristics_to_mapping_type(sect.Characteristics);

        for (auto i = page_start; i <= page_end; i++) {
            pages_protection[i] = prot;
        }
    }

    return nt_status::SUCCESS;
}

static uint64_t search_exports(span<uint8_t> dir, uint64_t offset, string_view name) {
    const auto& ied = *(IMAGE_EXPORT_DIRECTORY*)dir.data();

    if (ied.NumberOfNames == 0)
        return 0;

    auto names = span((uint32_t*)(dir.data() + ied.AddressOfNames - offset), ied.NumberOfNames);

    if ((uint8_t*)names.data() + names.size_bytes() > dir.data() + dir.size()) {
        debug_printk("invalid export directory\n");
        return 0;
    }

    auto name_ordinals = span((uint16_t*)(dir.data() + ied.AddressOfNameOrdinals - offset), ied.NumberOfNames);

    if ((uint8_t*)name_ordinals.data() + name_ordinals.size_bytes() > dir.data() + dir.size()) {
        debug_printk("invalid export directory\n");
        return 0;
    }

    auto funcs = span((uint32_t*)(dir.data() + ied.AddressOfFunctions - offset), ied.NumberOfFunctions);

    if ((uint8_t*)funcs.data() + funcs.size_bytes() > dir.data() + dir.size()) {
        debug_printk("invalid export directory\n");
        return 0;
    }

    optional<uint16_t> ord;

    for (unsigned int i = 0; i < names.size(); i++) {
        auto n = names[i];

        if (dir.data() + n - offset > dir.data() + dir.size()) {
            debug_printk("invalid export directory\n");
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
        debug_printk("Function ordinal out of range.\n");
        return 0;
    }

    return funcs[*ord];
}

uint64_t section::find_export(string_view name) {
    if (type != section_type::image) {
        debug_printk("Not an image.\n");
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
        debug_printk("allocate_va failed\n");
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
            debug_printk("section::query_info: unhandled info class ");
            debug_print_hex((unsigned int)section_information_class);
            debug_printk("\n");
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

size_t section::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    // FIXME - do lazy loading

    switch (type) {
        case section_type::image:
            // should already have been loaded in load_image
            return 0;

        case section_type::file:
            print("section::read_into_cache: FIXME - file mappings\n"); // FIXME
            return 0;

        case section_type::anonymous: {
            if (page_num >= this->num_pages)
                return 0;

            if (page_num + num_pages > this->num_pages)
                num_pages = this->num_pages - page_num;

            if (!cache.pin(page_num, num_pages, nullptr)) {
                print("page_cache::pin failed\n");
                return 0;
            }

            page_cache_entry* p;
            uint64_t read_pages = num_pages;

            for (uint64_t i = 0; i < num_pages; i++) {
                {
                    lock_guard lg(cache.cache_lock);

                    if (i == 0)
                        p = cache.find_page_cache_entry(page_num);
                    else
                        p = cache.next_page_cache_entry(*p);

                    if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                        if (!p->pfn) {
                            print("allocate_folio failed\n");
                            read_pages = i;
                            break;
                        }
                    }
                }

                // FIXME - lock?
                if (p->status == page_cache_status::allocated) {
                    memset(get_phys_address(*p), 0, PAGE_SIZE);
                    p->status = page_cache_status::clean;
                }
            }

            cache.unpin(page_num, num_pages);

            return read_pages;
        }

        default:
            return 0;
    }

#if 0
    uint64_t page;
    bool new_page = false;

    // FIXME - multiple pages

    {
        lock_guard lock(alloc_lock);

        page = pages_pa[page_num];

        if (!page) {
            if (type != section_type::anonymous) {
                print("page not set\n");
                return 0;
            }

            page = allocate_page();
            if (!page) {
                print("allocate_page failed\n");
                return 0;
            }

            pages_pa[page_num] = page;

            new_page = true;
        }
    }

    if (new_page)
        memset(phys_address(page), 0, PAGE_SIZE); // FIXME - allocate zeroed page

    {
        page_cache_entry* dest_p;

        {
            lock_guard lg(cache.cache_lock);

            dest_p = cache.find_page_cache_entry(page_num);
        }

        auto dest_va = get_phys_address(*dest_p);

        memcpy(dest_va, phys_address(page), PAGE_SIZE);
    }

    return 1;
#endif
}

size_t section::flush_cache(uint64_t page_num, uint64_t num_pages) {
    print("FIXME: section::flush_cache({}, {})\n", page_num, num_pages);
    // FIXME

    return 0;
}
