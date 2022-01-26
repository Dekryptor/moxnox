#include "kernel.h"
#include <pe.h>
#include <span>

using namespace std;

extern uint8_t __image_base__;

enum mapping_type characteristics_to_mapping_type(uint32_t c) {
    if (c & IMAGE_SCN_MEM_WRITE && !(c & IMAGE_SCN_MEM_EXECUTE))
        return mapping_type::read_write;
    else if (c & IMAGE_SCN_MEM_WRITE && c & IMAGE_SCN_MEM_EXECUTE)
        return mapping_type::read_write_execute;
    else if (!(c & IMAGE_SCN_MEM_WRITE) && !(c & IMAGE_SCN_MEM_EXECUTE))
        return mapping_type::read_only;
    else if (!(c & IMAGE_SCN_MEM_WRITE) && c & IMAGE_SCN_MEM_EXECUTE)
        return mapping_type::read_execute;

    return mapping_type::read_only;
}

void protect_kernel_pages() {
    const auto& dos_header = *(IMAGE_DOS_HEADER*)&__image_base__;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
        print("Incorrect DOS signature.\n");
        return;
    }

    const auto& nt_header = *(IMAGE_NT_HEADERS*)(&__image_base__ + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE) {
        print("Incorrect PE signature.\n");
        return;
    }

    span<const IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else {
        print("Unrecognized optional header signature.\n");
        return;
    }

    // first page
    protect_page((uintptr_t)&__image_base__, mapping_type::read_only);

    for (const auto& sect : sections) {
        if (sect.VirtualSize == 0)
            continue;

        auto pages = bytes_to_pages(sect.VirtualSize);
        auto va = (uintptr_t)&__image_base__ + sect.VirtualAddress;

        for (unsigned int i = 0; i < pages; i++) {
            protect_page(va, characteristics_to_mapping_type(sect.Characteristics));
            va += PAGE_SIZE;
        }
    }
}
