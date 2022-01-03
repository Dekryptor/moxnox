#pragma once

#include <efibind.h>
#include <efidef.h>
#include <efierr.h>
#include <efidevp.h>
#include <efiprot.h>
#include <eficon.h>
#include <efiapi.h>
#include <efipart.h>
#include <efigpt.h>
#include <string.h>
#include "partinfo.h"
#include <span>
#include <string_view>

#define KERNEL_ADDR 0xfffff80000000000

#define IMAGE_DOS_SIGNATURE             0x5a4d // "MZ"
#define IMAGE_NT_SIGNATURE              0x00004550 // "PE\0\0"

#define IMAGE_FILE_MACHINE_AMD64        0x8664

#define IMAGE_NT_OPTIONAL_HDR32_MAGIC   0x10b
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC   0x20b

#define IMAGE_DIRECTORY_ENTRY_BASERELOC     5

#define IMAGE_REL_BASED_ABSOLUTE    0
#define IMAGE_REL_BASED_HIGHLOW     3
#define IMAGE_REL_BASED_DIR64       10

struct IMAGE_DOS_HEADER {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
};

struct IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
};

struct IMAGE_OPTIONAL_HEADER32 {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
};

struct IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
};

struct IMAGE_NT_HEADERS {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    union {
        IMAGE_OPTIONAL_HEADER32 OptionalHeader32;
        IMAGE_OPTIONAL_HEADER64 OptionalHeader64;
    };
};

struct IMAGE_SECTION_HEADER {
    char Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} __attribute__((packed));

struct IMAGE_BASE_RELOCATION {
    uint32_t VirtualAddress;
    uint32_t SizeOfBlock;
};

struct HARDWARE_PTE {
    uint64_t Valid:1;
    uint64_t Write:1;
    uint64_t Owner:1;
    uint64_t WriteThrough:1;
    uint64_t CacheDisable:1;
    uint64_t Accessed:1;
    uint64_t Dirty:1;
    uint64_t LargePage:1;
    uint64_t Global:1;
    uint64_t CopyOnWrite:1;
    uint64_t Prototype:1;
    uint64_t reserved0:1;
    uint64_t PageFrameNumber:28;
    uint64_t reserved1:12;
    uint64_t SoftwareWsIndex:11;
    uint64_t NoExecute:1;
} __attribute__((packed));

struct RSDP_DESCRIPTOR {
    char signature[8];
    uint8_t checksum;
    char oem_id [6];
    uint8_t revision;
    uint32_t rsdt_physical_address;
    uint32_t length;
    uint64_t xsdt_physical_address;
    uint8_t extended_checksum;
    char reserved[3];
} __attribute__((packed));

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
}

enum class map_type { // FIXME - loaderblock.h
    mem_free,
    mem_reserved,
    mem_bootloader,
    mem_image,
    mem_paging_table,
    mem_stack,
    mem_acpi,
    mem_uefi_code,
    mem_uefi_data,
    mem_framebuffer
};

struct map_entry { // FIXME - loaderblock.h
    uint64_t addr;
    uint64_t len;
    enum map_type type;
} __attribute__((packed));

struct loader_block { // FIXME - loaderblock.h
    map_entry* map;
    uint32_t num_map_entries;
    uint64_t rsdt_pa;
    void* fb_pa;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pixels_per_scan_line;
} __attribute__((packed));

// efi.cpp
extern EFI_SYSTEM_TABLE* systable;
extern EFI_BOOT_SERVICES* bs;
void print(const char16_t* s);
void print_error(std::u16string_view func, EFI_STATUS status);

// pe.cpp
EFI_STATUS load_pe(uint8_t* data, size_t data_size, uint64_t va, uint64_t& entry_point, uint64_t& img_pa, size_t& img_size);

// mem.cpp
extern map_entry map[256];
extern unsigned int num_map_entries;
EFI_STATUS get_memory_map();
EFI_STATUS setup_paging(uint64_t& cr3);
EFI_STATUS add_map_entry(uint64_t start, uint64_t len, enum map_type type);
void print_hex(uint64_t num);

// efi.S
extern "C" void start_kernel(uint64_t entry_point, uint64_t pml4t, loader_block* lb);
