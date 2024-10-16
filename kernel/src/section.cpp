#include "kernel.h"
#include "objects.h"
#include "handle.h"
#include <pe.h>

using namespace std;

object_ref create_section(const optional<reference_wrapper<file>>& file, bool image, size_t size) {
    if (file && !image) {
        printk("FIXME - non-image sections\n");
        return {};
    }

    // FIXME - non-image sections

    auto sptr = new section;
    if (!sptr) {
        printk("new failed\n");
        return {};
    }

    auto& s = *sptr;

    if (file) {
        if (file->get().ino.type != object_type::inode) {
            printk("file pointed to object other than inode\n");
            return {};
        }

        s.ino = static_cast<inode&>(file->get().ino);
        s.ino->get().acquire();

        s.type = section_type::image;
        // FIXME - also work with ELFs

        if (!s.load_image()) {
            printk("load_image failed\n");
            s.release();
            return {};
        }
    } else {
        s.type = section_type::anonymous;
        s.num_pages = bytes_to_pages(size);

        if (s.num_pages == 0) {
            printk("anonymous section must specify a size\n");
            s.release();
            return {};
        }

        s.size = s.num_pages << PAGE_SHIFT;

        s.pages = (page_info*)malloc(sizeof(page_info) * s.num_pages);
        if (!s.pages) {
            printk("malloc failed\n");
            s.release();
            return {};
        }

        memset(s.pages, 0, sizeof(page_info) * s.num_pages);

        // FIXME - protection

        // FIXME - make sure allocate_page doesn't steal what we've reserved
    }

    return s;
}

section::~section() {
    if (ino)
        ino->get().release();

    free_pa();
}

void section::free_pa() {
    if (!pages)
        return;

    for (unsigned int i = 0; i < num_pages; i++) {
        if (pages[i].pa)
            free_page(pages[i].pa);
    }

    free(pages);
    pages = nullptr;
}

class scratch {
public:
    scratch() {
        pa = allocate_page();
        if (!pa) {
            printk("allocate_page failed\n");
            return;
        }

        va = allocate_va(1);
        if (!va) {
            printk("allocate_va failed\n");
            free_page(pa);
            return;
        }

        if (!map_page(pa, va, mapping_type::read_write)) {
            printk("map_page failed\n");
            free_va(va, 1);
            free_page(pa);
            return;
        }

        valid = true;
    }

    ~scratch() {
        if (!valid)
            return;

        unmap_page(va);
        free_va(va, 1);
        free_page(pa);
    }

    bool valid = false;
    uintptr_t pa, va;
};

bool section::load_image() {
    if (!ino) {
        printk("inode was NULL\n");
        return false;
    }

    auto& f = ino->get();
    uint64_t temp_va;

    {
        scratch s;

        auto buf = (uint8_t*)s.va;

        auto ret = f.read(span(buf, PAGE_SIZE), 0);
        if (ret == 0) {
            printk("inode read failed\n");
            return false;
        }

        if (ret < PAGE_SIZE) {
            printk("read less than 1 page\n");
            return false;
        }

        auto& dos_header = *(IMAGE_DOS_HEADER*)buf;

        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
            printk("invalid image header\n");
            return false;
        }

        if (dos_header.e_lfanew < sizeof(IMAGE_DOS_HEADER) || dos_header.e_lfanew > PAGE_SIZE - sizeof(IMAGE_NT_HEADERS)) {
            printk("invalid e_lfanew value\n");
            return false;
        }

        auto& nt_header = *(IMAGE_NT_HEADERS*)(buf + dos_header.e_lfanew);

        if (nt_header.Signature != IMAGE_NT_SIGNATURE) {
            printk("incorrect PE signature\n");
            return false;
        }

        if (nt_header.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
            printk("unsupported architecture\n");
            return false;
        }

        if (nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
            nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            printk("unrecognized optional header signature\n");
            return false;
        }

        // FIXME - check checksum

        // FIXME - check Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE

        uint32_t size_of_headers;

        if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
                printk("SizeOfOptionalHeader was smaller than expected\n");
                return false;
            }

            size = nt_header.OptionalHeader64.SizeOfImage;
            size_of_headers = nt_header.OptionalHeader64.SizeOfHeaders;
            preferred_base = nt_header.OptionalHeader64.ImageBase;
            stack_size = nt_header.OptionalHeader64.SizeOfStackReserve;
            entry_point = nt_header.OptionalHeader64.AddressOfEntryPoint;
        } else {
            if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)) {
                printk("SizeOfOptionalHeader was smaller than expected\n");
                return false;
            }

            size = nt_header.OptionalHeader32.SizeOfImage;
            size_of_headers = nt_header.OptionalHeader32.SizeOfHeaders;
            preferred_base = nt_header.OptionalHeader32.ImageBase;
            stack_size = nt_header.OptionalHeader32.SizeOfStackReserve;
            entry_point = nt_header.OptionalHeader32.AddressOfEntryPoint;
        }

        if (size == 0) {
            printk("size of image was 0\n");
            return false;
        }

        if (size_of_headers > size) {
            printk("SizeOfHeaders was more than SizeOfImage\n");
            return false;
        }

        if (size_of_headers > PAGE_SIZE) {
            printk("SizeOfHeaders was more than 1 page\n");
            return false;
        }

        num_pages = bytes_to_pages(size);
        size = num_pages << PAGE_SHIFT;

        pages = (page_info*)malloc(sizeof(page_info) * num_pages);
        if (!pages) {
            printk("malloc failed\n");
            return false;
        }

        // FIXME - allocate zeroed pages?

        for (unsigned int i = 0; i < num_pages; i++) {
            pages[i].pa = allocate_page();
            if (!pages[i].pa) {
                printk("allocate_page failed\n");

                for (unsigned int j = 0; j < i; j++) {
                    free_page(pages[j].pa);
                }

                free(pages);
                pages = nullptr;

                return false;
            }
        }

        temp_va = allocate_va(num_pages);
        if (!temp_va) {
            printk("allocate_va failed\n");
            free_pa();
            return false;
        }

        for (unsigned int i = 0; i < num_pages; i++) {
            if (!map_page(pages[i].pa, temp_va + (i << PAGE_SHIFT), mapping_type::read_write)) {
                printk("map_page failed\n");

                for (unsigned int j = 0; j < i; j++) {
                    unmap_page(temp_va + (j << PAGE_SHIFT));
                }

                free_va(temp_va, num_pages);
                free_pa();
                return false;
            }
        }

        memcpy((void*)temp_va, buf, size_of_headers);
        memset((void*)(temp_va + size_of_headers), 0, (num_pages << PAGE_SHIFT) - size_of_headers);
    }

    auto& dos_header = *(IMAGE_DOS_HEADER*)temp_va;
    auto& nt_header = *(IMAGE_NT_HEADERS*)(temp_va + dos_header.e_lfanew);
    span<IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    pages[0].protect = mapping_type::read_only;

    for (const auto& sect : sections) {
        auto section_size = max(sect.VirtualSize, sect.SizeOfRawData);

        if (section_size == 0)
            continue;

        if (sect.VirtualAddress + section_size > size) {
            printk("section overflow\n");
            free_va(temp_va, num_pages);
            free_pa();
            return false;
        }

        if (sect.PointerToRawData != 0) {
            auto read = f.read(span((uint8_t*)(temp_va + sect.VirtualAddress), section_size), sect.PointerToRawData);
            if (read < section_size) {
                printk("short read\n");
                free_va(temp_va, num_pages);
                free_pa();
                return false;
            }
        }

        auto page_start = bytes_to_pages(sect.VirtualAddress);
        auto page_end = bytes_to_pages(sect.VirtualAddress + section_size - 1);

        auto prot = characteristics_to_mapping_type(sect.Characteristics);

        for (auto i = page_start; i < page_end; i++) {
            pages[i].protect = prot;
        }
    }

    free_va(temp_va, num_pages);

    return true;
}

bool section::map(uint32_t offset, uint64_t va, bool readonly) {
    uint64_t page;
    bool new_page = false;

    {
        lock_guard lock(alloc_lock);

        page = pages[offset >> PAGE_SHIFT].pa;

        if (!page) {
            if (type != section_type::anonymous) {
                printk("page not set\n");
                return false;
            }

            page = allocate_page();
            if (!page) {
                printk("allocate_page failed\n");
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

        auto va = allocate_va(1);
        if (!va) {
            printk("allocate_va failed\n");
            return false;
        }

        if (!page) {
            // FIXME - handle missing page (load from disk if file, zero if anonymous)
            free_va(va, 1);
            return false;
        }

        if (!map_page(page, va, mapping_type::read_only)) {
            printk("map_page failed\n");
            free_va(va, 1);
            return false;
        }

        auto to_copy = min(size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        memcpy(ks_addr, (uint8_t*)va + (offset & (PAGE_SIZE - 1)), to_copy);

        unmap_page(va);
        free_va(va, 1);

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

        auto va = allocate_va(1);
        if (!va) {
            printk("allocate_va failed\n");
            return false;
        }

        if (!map_page(page, va, mapping_type::read_only)) {
            printk("map_page failed\n");
            free_va(va, 1);
            return false;
        }

        auto to_copy = min(size, PAGE_SIZE - (offset & (PAGE_SIZE - 1)));

        memcpy((uint8_t*)va + (offset & (PAGE_SIZE - 1)), ks_addr, to_copy);

        unmap_page(va);
        free_va(va, 1);

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
        printk("invalid export directory\n");
        return 0;
    }

    auto name_ordinals = span((uint16_t*)(dir.data() + ied.AddressOfNameOrdinals - offset), ied.NumberOfNames);

    if ((uint8_t*)name_ordinals.data() + name_ordinals.size_bytes() > dir.data() + dir.size()) {
        printk("invalid export directory\n");
        return 0;
    }

    auto funcs = span((uint32_t*)(dir.data() + ied.AddressOfFunctions - offset), ied.NumberOfFunctions);

    if ((uint8_t*)funcs.data() + funcs.size_bytes() > dir.data() + dir.size()) {
        printk("invalid export directory\n");
        return 0;
    }

    optional<uint16_t> ord;

    for (unsigned int i = 0; i < names.size(); i++) {
        auto n = names[i];

        if (dir.data() + n - offset > dir.data() + dir.size()) {
            printk("invalid export directory\n");
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
        printk("Function ordinal out of range.\n");
        return 0;
    }

    return funcs[*ord];
}

uint64_t section::find_export(string_view name) {
    if (type != section_type::image) {
        printk("Not an image.\n");
        return 0;
    }

    // map first page

    auto va = allocate_va(1);
    if (!va) {
        printk("allocate_va failed\n");
        return 0;
    }

    if (!map(0, va, true)) {
        printk("section::map failed\n");
        free_va(va, 1);
        return 0;
    }

    IMAGE_DATA_DIRECTORY exp_dir;
    bool found = false;

    const auto& dos_header = *(IMAGE_DOS_HEADER*)va;
    const auto& nt_header = *(IMAGE_NT_HEADERS*)(va + dos_header.e_lfanew);

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

    unmap_page(va);
    free_va(va, 1);

    if (!found) {
        printk("image has no export directory\n");
        return 0;
    }

    if (exp_dir.VirtualAddress == 0 || exp_dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) {
        printk("invalid export directory\n");
        return 0;
    }

    uint64_t exp_dir_start = exp_dir.VirtualAddress & ~(PAGE_SIZE - 1);
    uint64_t exp_dir_end = bytes_to_pages(exp_dir.VirtualAddress + exp_dir.Size) << PAGE_SHIFT;
    auto exp_dir_pages = bytes_to_pages(exp_dir_end - exp_dir_start);

    va = allocate_va(exp_dir_pages);
    if (!va) {
        printk("allocate_va failed\n");
        return 0;
    }

    for (unsigned int i = 0; i < exp_dir_pages; i++) {
        if (!map((uint32_t)(exp_dir_start + (i << PAGE_SHIFT)), va + (i << PAGE_SHIFT), true)) {
            printk("section::map failed\n");
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
    uint64_t size = maximum_size ? maximum_size->QuadPart : 0;
    optional<reference_wrapper<file>> f;

    // FIXME - desired_access
    // FIXME - object_attributes
    // FIXME - section_page_protection
    // FIXME - allocation_attributes that aren't SEC_IMAGE

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

    auto secth = create_section(f, allocation_attributes & SEC_IMAGE, size);

    if (f)
        f->get().release();

    if (!secth)
        return nt_status::INTERNAL_ERROR;

    auto h = p->add_handle(secth.get(), 0); // FIXME - access mask

    if (!h)
        return nt_status::INTERNAL_ERROR;

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
            sii.ImageCharacteristics = 0; // FIXME
            sii.DllCharacteristics = 0; // FIXME
            sii.Machine = 0; // FIXME
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
            printk("section::query_info: unhandled info class ");
            print_hex((unsigned int)section_information_class);
            printk("\n");
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
