.section ".boot", "a"

.set	section_align, 0x1000

file_start:
ccmp	x18, #0, #0xd, pl // instruction that happens to start with "MZ"
b	start_asm
.quad	0 // text_offset
.quad	image_end
.quad	0xa // flags (place anywhere, page size = 4K, little-endian)
.quad	0 // res2
.quad	0 // res3
.quad	0 // res4
.long	0x644d5241 // magic
.long	pe_start - file_start // e_lfanew

pe_start:
// IMAGE_NT_HEADERS
.long	0x4550 // Signature

// IMAGE_FILE_HEADER
.short	0xaa64 // Machine
.short	4 // NumberOfSections
.long	0 // TimeDateStamp
.long	0 // PointerToSymbolTable
.long	0 // NumberOfSymbols
.short	optional_header_end - optional_header_start // SizeOfOptionalHeader
.short	0x206 // Characteristics (IMAGE_FILE_DEBUG_STRIPPED | IMAGE_FILE_LINE_NUMS_STRIPPED | IMAGE_FILE_EXECUTABLE_IMAGE)

// IMAGE_OPTIONAL_HEADER64
optional_header_start:
.short	0x20b // Magic
.byte	0x2 // MajorLinkerVersion
.byte	0x14 // MinorLinkerVersion
.long	efi_text_size // SizeOfCode
.long	size_of_initialized_data // SizeOfInitializedData
.long	0 // SizeOfUninitializedData
.long	efi_entry_point // AddressOfEntryPoint
.long	0x1000 // BaseOfCode
.long	efi_text - 0x1000 // ImageBase
.long	0
.long	section_align // SectionAlignment
.long	0x200 // FileAlignment
.short	0 // MajorOperatingSystemVersion
.short	0 // MinorOperatingSystemVersion
.short	3 // MajorImageVersion
.short	0 // MinorImageVersion
.short	0 // MajorSubsystemVersion
.short	0 // MinorSubsystemVersion
.long	0 // Win32VersionValue
.long	size_of_image // SizeOfImage
.long	section_align // SizeOfHeaders
.long	0 // CheckSum
.short	0xa // Subsystem (EFI)
.short	0x100 // DllCharacteristics (IMAGE_DLLCHARACTERISTICS_NX_COMPAT)
.quad	0 // SizeOfStackReserve
.quad	0 // SizeOfStackCommit
.quad	0 // SizeOfHeapReserve
.quad	0 // SizeOfHeapCommit
.long	0 // LoaderFlags
.long	0 // NumberOfRvaAndSizes
optional_header_end:

// IMAGE_SECTION_HEADER
.ascii	".text"
.space	3
.long	efi_text_size // VirtualSize
.long	0x1000 // VirtualAddress
.long	efi_text_size // SizeOfRawData
.long	efi_text // PointerToRawData
.long	0 // PointerToRelocations
.long	0 // PointerToLinenumbers
.short	0 // NumberOfRelocations
.short	0 // NumberOfLinenumbers
.long	0x60000020 // Characteristics (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE)

// IMAGE_SECTION_HEADER
.ascii	".data"
.space	3
.long	efi_data_size // VirtualSize
.long	0x1000 + efi_text_size // VirtualAddress
.long	efi_data_size // SizeOfRawData
.long	efi_data // PointerToRawData
.long	0 // PointerToRelocations
.long	0 // PointerToLinenumbers
.short	0 // NumberOfRelocations
.short	0 // NumberOfLinenumbers
.long	0xc0000040 // Characteristics (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA)

// IMAGE_SECTION_HEADER
.ascii	".rdata"
.space	2
.long	efi_rdata_size // VirtualSize
.long	efi_rdata_vma // VirtualAddress
.long	efi_rdata_size // SizeOfRawData
.long	efi_rdata // PointerToRawData
.long	0 // PointerToRelocations
.long	0 // PointerToLinenumbers
.short	0 // NumberOfRelocations
.short	0 // NumberOfLinenumbers
.long	0x40000040 // Characteristics (IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA)

// IMAGE_SECTION_HEADER
.ascii	".payload"
.long	efi_payload_size // VirtualSize
.long	efi_payload_vma // VirtualAddress
.long	efi_payload_size // SizeOfRawData
.long	efi_payload // PointerToRawData
.long	0 // PointerToRelocations
.long	0 // PointerToLinenumbers
.short	0 // NumberOfRelocations
.short	0 // NumberOfLinenumbers
.long	0x40000040 // Characteristics (IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA)

.section ".stack"
.space	4096
top_of_stack:

.text
start_asm:
adrp	x1, top_of_stack
add		x1, x1, :lo12:top_of_stack
mov		sp, x1
adrp	x1, image_base
add		x1, x1, :lo12:image_base
bl		start

.section ".rdata"
.global image_size
image_size:
.long	image_size_val
