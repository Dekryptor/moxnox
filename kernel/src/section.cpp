#include "kernel.h"
#include "objects.h"
#include "pe.h"

using namespace std;

unique_handle create_section(handle_t h, bool image) {
    if (!image) {
        print("FIXME - non-image sections\n");
        return {};
    }

    // FIXME - non-image sections
    // FIXME - sections backed by page file

    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        print("No such handle\n");
        return {};
    }

    auto& obj = objrw->get();

    if (obj.type != object_type::inode) {
        // FIXME - exception
        print("Not an inode\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return {};
    }

    auto sptr = new section;
    if (!sptr) {
        print("new failed\n");
        return {};
    }

    auto& s = *sptr;

    s.ino = static_cast<inode&>(obj);

    // FIXME - also work with ELFs

    s.type = section_type::image;

    if (!s.load_image()) {
        print("load_image failed\n");
        s.release();
        return {};
    }

    auto hv = add_handle(s, 0); // FIXME - access mask

    s.release(); // add_handle increases refcount

    return unique_handle{hv};
}

section::~section() {
    if (ino)
        ino->get().release();

    free_pa();
}

void section::free_pa() {
    if (!pa)
        return;

    for (unsigned int i = 0; i < num_pages; i++) {
        free_page(pa[i]);
    }

    free(pa);
    pa = nullptr;
}

class scratch {
public:
    scratch() {
        pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");
            return;
        }

        va = allocate_va(1);
        if (!va) {
            print("allocate_va failed\n");
            free_page(pa);
            return;
        }

        if (!map_page(pa, va, mapping_type::read_write)) {
            print("map_page failed\n");
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
        print("inode was NULL\n");
        return false;
    }

    auto& f = ino->get();
    uint64_t temp_va;

    {
        scratch s;

        auto buf = (uint8_t*)s.va;

        auto ret = f.read(span(buf, PAGE_SIZE), 0);
        if (ret == 0) {
            print("inode read failed\n");
            return false;
        }

        if (ret < PAGE_SIZE) {
            print("read less than 1 page\n");
            return false;
        }

        auto& dos_header = *(IMAGE_DOS_HEADER*)buf;

        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
            print("invalid image header\n");
            return false;
        }

        if (dos_header.e_lfanew < sizeof(IMAGE_DOS_HEADER) || dos_header.e_lfanew > PAGE_SIZE - sizeof(IMAGE_NT_HEADERS)) {
            print("invalid e_lfanew value\n");
            return false;
        }

        auto& nt_header = *(IMAGE_NT_HEADERS*)(buf + dos_header.e_lfanew);

        if (nt_header.Signature != IMAGE_NT_SIGNATURE) {
            print("incorrect PE signature\n");
            return false;
        }

        if (nt_header.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
            print("unsupported architecture\n");
            return false;
        }

        if (nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
            nt_header.OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            print("unrecognized optional header signature\n");
            return false;
        }

        // FIXME - check checksum

        // FIXME - check Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE

        uint32_t size_of_headers;

        if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
                print("SizeOfOptionalHeader was smaller than expected\n");
                return false;
            }

            size = nt_header.OptionalHeader64.SizeOfImage;
            size_of_headers = nt_header.OptionalHeader64.SizeOfHeaders;
            preferred_base = nt_header.OptionalHeader64.ImageBase;
            entry_point = nt_header.OptionalHeader64.AddressOfEntryPoint;
        } else {
            if (nt_header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)) {
                print("SizeOfOptionalHeader was smaller than expected\n");
                return false;
            }

            size = nt_header.OptionalHeader32.SizeOfImage;
            size_of_headers = nt_header.OptionalHeader32.SizeOfHeaders;
            preferred_base = nt_header.OptionalHeader32.ImageBase;
            entry_point = nt_header.OptionalHeader32.AddressOfEntryPoint;
        }

        if (size == 0) {
            print("size of image was 0\n");
            return false;
        }

        if (size_of_headers > size) {
            print("SizeOfHeaders was more than SizeOfImage\n");
            return false;
        }

        if (size_of_headers > PAGE_SIZE) {
            print("SizeOfHeaders was more than 1 page\n");
            return false;
        }

        num_pages = bytes_to_pages(size);
        size = num_pages << PAGE_SHIFT;

        pa = (uint64_t*)malloc(sizeof(uint64_t) * num_pages);
        if (!pa) {
            print("malloc failed\n");
            return false;
        }

        // FIXME - allocate zeroed pages?

        for (unsigned int i = 0; i < num_pages; i++) {
            pa[i] = allocate_page();
            if (!pa[i]) {
                print("allocate_page failed\n");

                for (unsigned int j = 0; j < i; j++) {
                    free_page(pa[j]);
                }

                free(pa);
                pa = nullptr;

                return false;
            }
        }

        temp_va = allocate_va(num_pages);
        if (!temp_va) {
            print("allocate_va failed\n");
            free_pa();
            return false;
        }

        for (unsigned int i = 0; i < num_pages; i++) {
            if (!map_page(pa[i], temp_va + (i << PAGE_SHIFT), mapping_type::read_write)) {
                print("map_page failed\n");

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

    for (const auto& sect : sections) {
        auto section_size = min(sect.VirtualSize, sect.SizeOfRawData);

        if (section_size == 0 || sect.PointerToRawData == 0)
            continue;

        if (sect.VirtualAddress + section_size > size) {
            print("section overflow\n");
            free_va(temp_va, num_pages);
            free_pa();
            return false;
        }

        auto read = f.read(span((uint8_t*)(temp_va + sect.VirtualAddress), section_size), sect.PointerToRawData);
        if (read < section_size) {
            print("short read\n");
            free_va(temp_va, num_pages);
            free_pa();
            return false;
        }
    }

    free_va(temp_va, num_pages);

    return true;
}

bool section::map(uint32_t offset, uint64_t va) {
    // FIXME - correct mapping type

    if (!map_page(pa[offset >> PAGE_SHIFT], va, mapping_type::read_write_execute))
        return false;

    return true;
}
