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

static bool allocate_memory(size_t size, void** addr) {
    // FIXME - make sure we only grab a physical address within first 4GB

    for (unsigned int i = 0; i < num_map_entries; i++) {
        if (map[i].type == mem_free && map[i].len >= size) {
            *addr = (void*)(uintptr_t)map[i].addr;

            return add_map_entry(map[i].addr, size, mem_image);
        }
    }

    print("Could not find 0x");
    print_hex(size);
    print(" bytes free for kernel image.\n");

    return false;
}

bool load_pe(void* data, size_t data_size) {
    IMAGE_NT_HEADERS* nt_header;
    uint8_t* addr;
    size_t size;

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

    allocate_memory(size, (void**)&addr);

    print("Loading at physical address ");
    print_hex((uintptr_t)addr);
    print("\n");

    // FIXME - copy sections
    // FIXME - relocations

    return false;
}
