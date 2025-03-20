#include "kernel.h"
#include "handle.h"
#include "spinlock.h"
import rbtree;
#include "visit_object.h"
#include <pe.h>

import format;
import print;
import mem;
import config;
import pe;

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

        s.pages = (page_info*)malloc(sizeof(page_info) * s.num_pages);
        if (!s.pages) {
            debug_printk("malloc failed\n");
            s.release();
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        memset(s.pages, 0, sizeof(page_info) * s.num_pages);

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
    if (!pages)
        return;

    for (unsigned int i = 0; i < num_pages; i++) {
        if (pages[i].pa)
            free_folio(pages[i].pa);
    }

    free(pages);
    pages = nullptr;
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

    pages = (page_info*)malloc(sizeof(page_info) * num_pages);
    if (!pages) {
        debug_printk("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    // FIXME - allocate zeroed pages?

    for (unsigned int i = 0; i < num_pages; i++) {
        pages[i].pa = allocate_page();
        if (!pages[i].pa) {
            debug_printk("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                free_folio(pages[j].pa);
            }

            free(pages);
            pages = nullptr;

            return nt_status::INSUFFICIENT_RESOURCES;
        }
    }

    header_pages = bytes_to_pages(size_of_headers);

    for (unsigned int i = 0; i < header_pages; i++) {
        size_t to_copy;

        if (i == header_pages - 1 && size_of_headers % PAGE_SIZE)
            to_copy = size_of_headers % PAGE_SIZE;
        else
            to_copy = PAGE_SIZE;

        memcpy(phys_address(pages[i].pa), buf + (i << PAGE_SHIFT),
                to_copy);

        if (to_copy < PAGE_SIZE) {
            memset((uint8_t*)phys_address(pages[i].pa) + to_copy, 0,
                    PAGE_SIZE - to_copy);
        }
    }

    for (unsigned int i = header_pages; i < num_pages; i++) {
        memset(phys_address(pages[i].pa), 0, PAGE_SIZE);
    }

    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    pages[0].protect = mapping_type::read_only;

    // FIXME - we should be creating our own page cache for this

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
                free_pa();
                return nt_status::INVALID_IMAGE_FORMAT;
            }

            do {
                auto pinned_pages = (uint32_t)f.cache.read_into_cache_and_pin(file_start_page + page_num, file_pages_left);

                if (pinned_pages == 0) {
                    debug_printk("inode::read_into_cache_and_pin did not pin any pages\n");
                    free_pa();
                    return nt_status::INTERNAL_ERROR;
                }

                for (unsigned int i = 0; i < pinned_pages; i++) {
                    span<uint8_t> sp;

                    if (page_num != 0 && offset != 0) {
                        sp = span((uint8_t*)phys_address(pages[(sect.VirtualAddress >> PAGE_SHIFT) + page_num - 1].pa) + (PAGE_SIZE - offset),
                                  min(offset, bytes_left));

                        f.cache.copy_into_buffer(file_start_page + page_num, 1, sp, 0);
                        bytes_left -= sp.size();

                        if (bytes_left == 0)
                            break;
                    }

                    sp = span((uint8_t*)phys_address(pages[(sect.VirtualAddress >> PAGE_SHIFT) + page_num].pa),
                              min(PAGE_SIZE - offset, bytes_left));

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
            pages[i].protect = prot;
        }
    }

    return nt_status::SUCCESS;
}

bool section::map(uint32_t offset, uint64_t va, bool readonly) {
    uint64_t page;
    bool new_page = false;

    {
        lock_guard lock(alloc_lock);

        page = pages[offset >> PAGE_SHIFT].pa;

        if (!page) {
            if (type != section_type::anonymous) {
                debug_printk("page not set\n");
                return false;
            }

            page = allocate_page();
            if (!page) {
                debug_printk("allocate_page failed\n");
                return false;
            }

            pages[offset >> PAGE_SHIFT].pa = page;

            new_page = true;
        }
    }

    {
        // FIXME - process mapping lock
        auto prot = pages[offset >> PAGE_SHIFT].protect;

        if (readonly) {
            switch (prot) {
                case mapping_type::read_write:
                case mapping_type::write_combine:
                case mapping_type::no_cache:
                    prot = mapping_type::read_only;
                    break;

                case mapping_type::read_write_execute:
                    prot = mapping_type::read_execute;
                    break;

                default:
                    break;
            }
        }

        if (!map_page(pages[offset >> PAGE_SHIFT].pa, va, prot))
            return false;

        if (new_page) // FIXME - allocate zeroed page?
            memset((void*)va, 0, PAGE_SIZE);
    }

    return true;
}

bool section::copy_from(uint32_t offset, void* ks_addr, uint32_t size) {
    lock_guard lock(alloc_lock);

    while (true) {
        auto page = pages[offset >> PAGE_SHIFT].pa;

        if (!page) {
            // FIXME - handle missing page (load from disk if file, zero if anonymous)
            return false;
        }

        auto va = phys_address(page);

        auto to_copy = (uint32_t)min((size_t)size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        memcpy(ks_addr, (uint8_t*)va + (offset & (PAGE_SIZE - 1)), to_copy);

        if (to_copy == size)
            return true;

        ks_addr = (uint8_t*)ks_addr + to_copy;
        offset += to_copy;
        size -= to_copy;
    }
}

bool section::copy_to(uint32_t offset, const void* ks_addr, uint32_t size) {
    lock_guard lock(alloc_lock);

    while (true) {
        auto page = pages[offset >> PAGE_SHIFT].pa;

        if (!page) {
            // FIXME - handle missing page (load from disk if file, zero if anonymous)
            return false;
        }

        // FIXME - page protection (copy-on-write etc.)

        auto va = phys_address(page);

        auto to_copy = (uint32_t)min((size_t)size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        memcpy((uint8_t*)va + (offset & (PAGE_SIZE - 1)), ks_addr, to_copy);

        if (to_copy == size)
            return true;

        ks_addr = (uint8_t*)ks_addr + to_copy;
        offset += to_copy;
        size -= to_copy;
    }
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
    uint64_t page;

    {
        lock_guard lock(alloc_lock);
        page = pages[0].pa;
    }

    if (!page) {
        debug_printk("first page of image not set\n");
        return 0;
    }

    {
        auto va = phys_address(page);

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
            debug_printk("image has no export directory\n");
            return 0;
        }
    }

    if (exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) {
        debug_printk("invalid export directory\n");
        return 0;
    }

    uint64_t exp_dir_start = exp_dir.VirtualAddress & ~(PAGE_SIZE - 1);
    uint64_t exp_dir_end = bytes_to_pages(exp_dir.VirtualAddress + exp_dir.Size) << PAGE_SHIFT;
    auto exp_dir_pages = bytes_to_pages(exp_dir_end - exp_dir_start);

    // FIXME - avoid doing mapping here?

    auto va = allocate_va(exp_dir_pages);
    if (!va) {
        debug_printk("allocate_va failed\n");
        return 0;
    }

    for (unsigned int i = 0; i < exp_dir_pages; i++) {
        if (!map((uint32_t)(exp_dir_start + (i << PAGE_SHIFT)), va + (i << PAGE_SHIFT), true)) {
            debug_printk("section::map failed\n");
            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }
            free_va(va, exp_dir_pages);
            return 0;
        }
    }

    auto ret = search_exports(span((uint8_t*)va + exp_dir.VirtualAddress - exp_dir_start, exp_dir.Size),
                              exp_dir.VirtualAddress, name);

    for (unsigned int i = 0; i < exp_dir_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, exp_dir_pages);

    return ret;
}

NTSTATUS section::protect(uint32_t offset, uint32_t page_count, enum mapping_type prot,
                          enum mapping_type& old_prot) {
    auto p = &pages[offset >> PAGE_SHIFT];

    for (unsigned int i = 0; i < page_count; i++) {
        if (i == 0)
            old_prot = p->protect;

        p->protect = prot;
        p++;
    }

    return nt_status::SUCCESS;
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
        auto objrw = deref_handle2(file_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        visit_obj(objrw.get(), [&](auto&& o) {
            if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>) {
                if (o.ino.type == object_type::inode)
                    f = o;
                else
                    o.release();
            } else
                o.release();
        });

        if (!f)
            return nt_status::INVALID_HANDLE;
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
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();
    } else {
        auto objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw->get();

        if (obj.type != object_type::process) {
            visit_obj(obj, [](auto&& o) {
                o.release();
            });

            return nt_status::OBJECT_TYPE_MISMATCH;
        }

        // FIXME - check access rights

        p = static_cast<process*>(&objrw->get());
    }

    auto objrw = deref_handle(section_handle);
    if (!objrw) {
        p->release();
        return nt_status::INVALID_HANDLE;
    }

    auto& obj = objrw->get();

    if (obj.type != object_type::section) {
        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        p->release();

        return nt_status::OBJECT_TYPE_MISMATCH;
    }

    auto& sect = static_cast<section&>(obj);

    auto ret = p->map_view_of_section(sect, base_address, zero_bits, commit_size, section_offset,
                                      *view_size, inherit_disposition, allocation_type,
                                      win32_protect);

    sect.release();
    p->release();

    return ret;
}

NTSTATUS NtUnmapViewOfSection(HANDLE process_handle, void* base_address) {
    process* p;

    if (process_handle == NtCurrentProcess()) {
        auto t = get_current_thread();

        if (!t->proc)
            return nt_status::INVALID_PARAMETER;

        p = t->proc;
        p->acquire();
    } else {
        auto objrw = deref_handle(process_handle);
        if (!objrw)
            return nt_status::INVALID_HANDLE;

        auto& obj = objrw->get();

        if (obj.type != object_type::process) {
            visit_obj(obj, [](auto&& o) {
                o.release();
            });

            return nt_status::OBJECT_TYPE_MISMATCH;
        }

        // FIXME - check access rights

        p = static_cast<process*>(&objrw->get());
    }

    auto ret = p->unmap_view_of_section(base_address);

    p->release();

    return ret;
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
    section* s;

    auto objrw = deref_handle(section_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw->get();

    if (obj.type != object_type::section) {
        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return nt_status::OBJECT_TYPE_MISMATCH;
    }

    // FIXME - check access rights

    s = static_cast<section*>(&objrw->get());

    auto ret = s->query_info(section_information_class, span((uint8_t*)section_information, section_information_length),
                             return_length);

    s->release();

    return ret;
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
