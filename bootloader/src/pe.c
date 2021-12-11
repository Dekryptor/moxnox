#include "bootloader.h"

static bool check_header(uint8_t* data, size_t size, IMAGE_NT_HEADERS** nth) {
    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)data;
    IMAGE_NT_HEADERS* nt_header;

    if (size < sizeof(IMAGE_DOS_HEADER)) {
        print("Image was shorter than IMAGE_DOS_HEADER.\n");
        return false;
    }

    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        print("Incorrect DOS signature.\n");
        return false;
    }

    nt_header = (IMAGE_NT_HEADERS*)(data + dos_header->e_lfanew);

    // FIXME - check no overflow

    if (nt_header->Signature != IMAGE_NT_SIGNATURE) {
        print("Incorrect PE signature.\n");
        return false;
    }

    if (nt_header->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        print("Unsupported architecture.\n");
        return false;
    }

    // FIXME - make sure optional header size is at least minimum

    if (nt_header->OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt_header->OptionalHeader32.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        print("Unrecognized optional header signature.\n");
        return false;
    }

    // FIXME - check checksum

    // FIXME - check Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE

    *nth = nt_header;

    return true;
}

static bool do_relocations(uint8_t* data, uint64_t va, IMAGE_NT_HEADERS* nt_header) {
    IMAGE_BASE_RELOCATION* reloc;
    uint32_t size, count;
    uint16_t* addr;
    uint64_t base;

    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (nt_header->OptionalHeader64.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return true;
        }

        size = nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

        reloc = (IMAGE_BASE_RELOCATION*)(data + nt_header->OptionalHeader64.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

        base = nt_header->OptionalHeader64.ImageBase;
    } else {
        if (nt_header->OptionalHeader32.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC ||
            nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0 ||
            nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            return true;
        }

        size = nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

        reloc = (IMAGE_BASE_RELOCATION*)(data + nt_header->OptionalHeader32.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

        base = nt_header->OptionalHeader32.ImageBase;
    }

    do {
        uint32_t* ptr;

        if (size < reloc->SizeOfBlock)
            return true;

        // FIXME - check not out of bounds
        ptr = (uint32_t*)(data + reloc->VirtualAddress);

        addr = (uint16_t*)((uint8_t*)reloc + sizeof(IMAGE_BASE_RELOCATION));
        count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);

        for (unsigned int i = 0; i < count; i++) {
            uint16_t offset = addr[i] & 0xfff;
            uint16_t type = addr[i] >> 12;

            switch (type) {
                case IMAGE_REL_BASED_ABSOLUTE:
                    // nop
                break;

                case IMAGE_REL_BASED_HIGHLOW:
                {
                    uint32_t* ptr2 = (uint32_t*)((uint8_t*)ptr + offset);

                    *ptr2 = *ptr2 - base + va;
                    break;
                }

                case IMAGE_REL_BASED_DIR64:
                {
                    uint64_t* ptr2 = (uint64_t*)((uint8_t*)ptr + offset);

                    *ptr2 = *ptr2 - base + va;
                    break;
                }

                default:
                    print("Unsupported relocation type ");
                    print_hex(type);
                    print(".\n");

                    return false;
            }
        }

        size -= reloc->SizeOfBlock;

        if (size < sizeof(IMAGE_BASE_RELOCATION))
            return true;

        reloc = (IMAGE_BASE_RELOCATION*)((uint8_t*)reloc + reloc->SizeOfBlock);
    } while (true);

    return true;
}

bool load_pe(void* data, size_t data_size, uint64_t va, uint64_t* entry_point) {
    IMAGE_NT_HEADERS* nt_header;
    uint8_t* addr;
    size_t size;
    IMAGE_SECTION_HEADER* sections;

    print("Loading image\n");

    if (!check_header(data, data_size, &nt_header))
        return false;

    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        size = nt_header->OptionalHeader64.SizeOfImage;
    else
        size = nt_header->OptionalHeader32.SizeOfImage;

    print("PE image is 0x");
    print_hex(size);
    print(" bytes \n");

    if (!allocate_memory(size, (void**)&addr, mem_image)) {
        print("Could not find 0x");
        print_hex(size);
        print(" bytes free for kernel image.\n");
        return false;
    }

    print("Loading at physical address ");
    print_hex((uintptr_t)addr);
    print("\n");

    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        sections = (IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header->OptionalHeader64 + nt_header->FileHeader.SizeOfOptionalHeader);

        // copy header
        memcpy(addr, data, nt_header->OptionalHeader64.SizeOfHeaders);
    } else {
        sections = (IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header->OptionalHeader32 + nt_header->FileHeader.SizeOfOptionalHeader);

        // copy header
        memcpy(addr, data, nt_header->OptionalHeader32.SizeOfHeaders);
    }

    for (unsigned int i = 0; i < nt_header->FileHeader.NumberOfSections; i++) {
        uint32_t section_size;

        // FIXME - check no overruns

        section_size = sections[i].VirtualSize;

        if (sections[i].SizeOfRawData < section_size)
            section_size = sections[i].SizeOfRawData;

        if (section_size > 0 && sections[i].PointerToRawData != 0)
            memcpy(addr + sections[i].VirtualAddress, data + sections[i].PointerToRawData, section_size);

        if (section_size < sections[i].VirtualSize) // if short, pad with zeroes
            memset(addr + sections[i].VirtualAddress + section_size, 0, sections[i].VirtualSize - section_size);
    }

    if (!do_relocations(data, va, nt_header))
        return false;

    if (nt_header->OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        *entry_point = nt_header->OptionalHeader64.AddressOfEntryPoint;
    else
        *entry_point = nt_header->OptionalHeader32.AddressOfEntryPoint;

    *entry_point += va;

    return true;
}
