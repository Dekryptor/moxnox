#include <version.h>

.intel_syntax noprefix
.code16

.equ E820_COUNT, 128
.equ E820_SIZE, 20

.equ MAX_VBE_MODES, 246

.equ VBE_INFO_SIZE, 50
.equ VBE_INFO_MODES_OFFSET, 14
.equ VBE_INFO_MODES_SEGMENT, 16

.equ VBE_MODE_INFO_ATTRIBUTES, 0
.equ VBE_MODE_INFO_WIDTH, 18
.equ VBE_MODE_INFO_HEIGHT, 20
.equ VBE_MODE_INFO_BPP, 25
.equ VBE_MODE_INFO_MEMORY_MODEL, 27
.equ VBE_MODE_INFO_RED_MASK, 31
.equ VBE_MODE_INFO_BLUE_MASK, 35

.equ VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE, 0x0080

.equ MM_TEXT, 0
.equ MM_DIRECT, 6

.section ".boot", "a"

.set	section_align, 0x1000

file_start:
# IMAGE_DOS_HEADER
.short	0x5a4d # e_magic
.short	0 # e_cblp
.short	0 # e_cp
.short	0 # e_crlc
.short	0 # e_cparhdr
.short	0 # e_minalloc
.short	0 # e_maxalloc
.short	0 # e_ss
.short	0 # e_sp
.short	0 # e_csum
.short	0 # e_ip
.short	0 # e_cs
.short	0 # e_lfarlc
.short	0 # e_ovno
.short	0 # e_res[4]
.short	0
.short	0
.short	0
.short	0 # e_oemid
.short	0 # e_oeminfo
.short	0 # e_res2[10]
.short	0
.short	0
.short	0
.short	0
.short	0
.short	0
.short	0
.short	0
.short	0
.long	pe_start - file_start # e_lfanew

pe_start:
# IMAGE_NT_HEADERS
.long	0x4550 # Signature

# IMAGE_FILE_HEADER
.short	0x8664 # Machine
.short	3 # NumberOfSections
.long	0 # TimeDateStamp
.long	0 # PointerToSymbolTable
.long	0 # NumberOfSymbols
.short	optional_header_end - optional_header_start # SizeOfOptionalHeader
.short	0x206 # Characteristics (IMAGE_FILE_DEBUG_STRIPPED | IMAGE_FILE_LINE_NUMS_STRIPPED | IMAGE_FILE_EXECUTABLE_IMAGE)

# IMAGE_OPTIONAL_HEADER64
optional_header_start:
.short	0x20b # Magic
.byte	0x2 # MajorLinkerVersion
.byte	0x14 # MinorLinkerVersion
.long	efi_text_size # SizeOfCode
.long	size_of_initialized_data # SizeOfInitializedData
.long	0 # SizeOfUninitializedData
.long	efi_entry_point # AddressOfEntryPoint
.long	0x1000 # BaseOfCode
.long	efi_text - 0x1000 # ImageBase
.long	0
.long	section_align # SectionAlignment
.long	0x200 # FileAlignment
.short	0 # MajorOperatingSystemVersion
.short	0 # MinorOperatingSystemVersion
.short	3 # MajorImageVersion
.short	0 # MinorImageVersion
.short	0 # MajorSubsystemVersion
.short	0 # MinorSubsystemVersion
.long	0 # Win32VersionValue
.long	size_of_image # SizeOfImage
.long	section_align # SizeOfHeaders
.long	0 # CheckSum
.short	0xa # Subsystem (EFI)
.short	0x100 # DllCharacteristics (IMAGE_DLLCHARACTERISTICS_NX_COMPAT)
.quad	0 # SizeOfStackReserve
.quad	0 # SizeOfStackCommit
.quad	0 # SizeOfHeapReserve
.quad	0 # SizeOfHeapCommit
.long	0 # LoaderFlags
.long	0 # NumberOfRvaAndSizes
optional_header_end:

# IMAGE_SECTION_HEADER
.ascii	".text"
.space	3
.long	efi_text_size # VirtualSize
.long	0x1000 # VirtualAddress
.long	efi_text_size # SizeOfRawData
.long	efi_begin # PointerToRawData
.long	0 # PointerToRelocations
.long	0 # PointerToLinenumbers
.short	0 # NumberOfRelocations
.short	0 # NumberOfLinenumbers
.long	0x60000020 # Characteristics (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE)

# IMAGE_SECTION_HEADER
.ascii	".data"
.space	3
.long	efi_data_size # VirtualSize
.long	0x1000 + efi_text_size # VirtualAddress
.long	efi_data_size # SizeOfRawData
.long	efi_data_begin # PointerToRawData
.long	0 # PointerToRelocations
.long	0 # PointerToLinenumbers
.short	0 # NumberOfRelocations
.short	0 # NumberOfLinenumbers
.long	0xc0000040 # Characteristics (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA)

# IMAGE_SECTION_HEADER
.ascii	".rdata"
.space	2
.long	efi_rdata_size # VirtualSize
.long	efi_rdata_vma # VirtualAddress
.long	efi_rdata_size # SizeOfRawData
.long	efi_rdata_begin # PointerToRawData
.long	0 # PointerToRelocations
.long	0 # PointerToLinenumbers
.short	0 # NumberOfRelocations
.short	0 # NumberOfLinenumbers
.long	0x40000040 # Characteristics (IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA)

.space  0xb1, 0xff

.byte	setup_sects - 1
.short	1 # root_flags (ROOT_RDONLY)
.long	syssize
.short	0 # ram_size (0)
.short	0xffff # vid_mode (SVGA_MODE)
.short	0 # root_dev (0)
.short	0xaa55 # boot_flag

.globl start
start:
.byte	0xeb # jump
.byte	start_of_setup - 1f
1:

.ascii	"HdrS" # header
.short	0x204 # FIXME - version (20f)
.long	0 # realmode_swtch
.short	0x1000 # start_sys_seg (SYSSEG)
.short	kernel_version - start
.byte	0 # type_of_loader
.byte	1 # loadflags (LOADED_HIGH)
.short	0x8000 # setup_move_size
.long	0x100000 # code32_start
.long	0 # ramdisk_image
.long	0 # ramdisk_size
.long	0 # bootsect_kludge
.short	0 # heap_end_ptr (51c0)
.byte	0 # ext_loader_ver
.byte	0 # ext_loader_type
.cmd_line_ptr:
.long	0 # cmd_line_ptr
.long	0x7fffffff # initrd_addr_max
.long	0x1000000 # kernel_alignment (CONFIG_PHYSICAL_ALIGN)
.byte	0 # FIXME - relocatable_kernel (1)
.byte	0x15 # FIXME - min_alignment (MIN_KERNEL_ALIGN_LG2)
.short	0 # FIXME - xloadflags (2b)
.long	0 # FIXME - cmdline_size (7ff)
.long	0 # hardware_subarch
.quad	0 # hardware_subarch_data
.long	0 # FIXME - payload_offset (2c4)
.long	0 # FIXME - payload_length (ab4da8)
.quad	0 # setup_data
.quad	0x1000000 # pref_address (LOAD_PHYSICAL_ADDR)
.long	0 # FIXME - init_size (2a63000)
.long	0 # FIXME - handover_offset (ad4a09)

start_of_setup:

mov	ax, ds
mov	es, ax
cld

mov	dx, sp
and	dx, ~3
mov	ss, ax
movzx	esp, dx
sti

# zero BSS
mov	di, OFFSET __rm_bss_start
mov	cx, OFFSET _end + 3
xor	eax, eax
sub	cx, di
shr	cx, 2
rep stosd

push	ds
push	OFFSET 1f
lret

1:
detect_memory:
mov	di, OFFSET e820_entries
xor	ebx, ebx

.loop:
mov	edx, 0x534d4150
mov	eax, 0xe820
mov	ecx, E820_SIZE
int	0x15

cmp	eax, 0x534d4150
jne	.out # failed, eax not 534d4150
inc	DWORD PTR [e820_count]
jc	.out # end, carry bit set
test	ebx, ebx
je	.out # end, ebx is 0
cmp	DWORD PTR [e820_count], E820_SIZE # end of array
jge	.out

add	di, E820_SIZE
jmp	.loop

.out:
jmp	real_mode_main

.global vbe_get_info
vbe_get_info:
push	di
mov	ax, 0x4f00
lea	di, vbe_info
int	0x10

cmp	ax, 0x4f
jne	.vbe_get_info_fail

mov	eax, DWORD PTR [vbe_info]
cmp	eax, 0x41534556
jne	.vbe_get_info_fail

mov	ax, WORD PTR [vbe_info+4]
cmp	ax, 0x0300
jne	.vbe_get_info_fail

mov	ax, 1
jmp	.vbe_get_info_end

.vbe_get_info_fail:
xor	ax, ax

.vbe_get_info_end:
pop	di
ret

.global vbe_get_edid
vbe_get_edid:
push	bx
push	di

mov	ax, 0x4f15
mov	bl, 0x01
xor	cx, cx
xor	dx, dx
lea	di, vbe_edid
int	0x10

cmp	ax, 0x4f
jne	0f
mov	BYTE PTR [vbe_edid_valid], 1

0:
pop	di
pop	bx
ret

.global vbe_get_modes
vbe_get_modes:
push	di
push	si

mov	fs, WORD PTR [vbe_info+VBE_INFO_MODES_SEGMENT]
mov	dx, WORD PTR [vbe_info+VBE_INFO_MODES_OFFSET]
and	edx, 0xffff
push	ds
pop	es
lea	di, [vbe_modes+2]

.vbe_get_modes_loop:
mov	cx, WORD PTR fs:[edx]
cmp	cx, 0xffff
je	.vbe_get_modes_end
cmp	WORD PTR [vbe_num_modes], MAX_VBE_MODES
je	.vbe_get_modes_end

mov	WORD PTR [di - 2], cx
mov	ax, 0x4f01
int	0x10
cmp	ax, 0x4f
jne	.vbe_get_modes_end

# skip if mode not supported
mov	si, WORD PTR [di+VBE_MODE_INFO_ATTRIBUTES]
test	si, 0x1
je	.vbe_get_modes_next

# # skip if not linear framebuffer (FIXME)
# test	si, 0x80
# je	.vbe_get_modes_next

inc	WORD PTR [vbe_num_modes]
add	di, 2 + VBE_INFO_SIZE

.vbe_get_modes_next:
add	dx, 2
jmp	.vbe_get_modes_loop

.vbe_get_modes_end:
pop	si
pop	di
ret

vbe_set_mode:
mov	ax, 0x4f02
mov	bx, cx
or	bx, 0x4000 # set "Use LFB" bit
int	0x10
# FIXME - check for success
ret

vbe_choose_graphics_mode:
push	di
push	esi

# use preferred resolution in EDID if not overridden
cmp	WORD PTR [vbe_preferred_video_width], 0
jne	0f
cmp	WORD PTR [vbe_preferred_video_height], 0
jne	0f
cmp	BYTE PTR [vbe_edid_valid], 0
je	0f

movzx	dx, BYTE PTR [vbe_edid+0x3a]
and	dx, 0xf0
shl	dx, 4
movzx	ax, BYTE PTR [vbe_edid+0x38]
or	ax, dx
mov	WORD PTR [vbe_preferred_video_width], ax

movzx	dx, BYTE PTR [vbe_edid+0x3d]
and	dx, 0xf0
shl	dx, 4
movzx	ax, BYTE PTR [vbe_edid+0x3b]
or	ax, dx
mov	WORD PTR [vbe_preferred_video_height], ax

0:
xor	dx, dx
lea	di, [vbe_modes]

.vbe_choose_graphics_mode_loop:
cmp	dx, WORD PTR [vbe_num_modes]
je	.vbe_choose_graphics_mode_after_loop

# continue if memory model is not direct
cmp	BYTE PTR [di+2+VBE_MODE_INFO_MEMORY_MODEL], MM_DIRECT
jne	.vbe_choose_graphics_mode_continue

# continue if width == 0 || height == 0
cmp	WORD PTR [di+2+VBE_MODE_INFO_WIDTH], 0
je	.vbe_choose_graphics_mode_continue
cmp	WORD PTR [di+2+VBE_MODE_INFO_HEIGHT], 0
je	.vbe_choose_graphics_mode_continue

# continue if not linear framebuffer
mov	ax, WORD PTR [di+2+VBE_MODE_INFO_ATTRIBUTES]
or	ax, VBE_MODE_ATTRIBUTE_LINEAR_FRAME_BUFFER_MODE
cmp	ax, 0
je	.vbe_choose_graphics_mode_continue

# continue if not 32 bits per pixel
cmp	BYTE PTR [di+2+VBE_MODE_INFO_BPP], 32
jne	.vbe_choose_graphics_mode_continue

# continue if not BGR
cmp	DWORD PTR [di+2+VBE_MODE_INFO_RED_MASK], 0x08081008
jne	.vbe_choose_graphics_mode_continue
cmp	WORD PTR [di+2+VBE_MODE_INFO_BLUE_MASK], 0x0008
jne	.vbe_choose_graphics_mode_continue

# # break if width == vbe_preferred_video_width && height == vbe_preferred_video_height
mov	si, WORD PTR [vbe_preferred_video_width]
cmp	WORD PTR [di+2+VBE_MODE_INFO_WIDTH], si
jne	1f
mov	si, WORD PTR [vbe_preferred_video_height]
cmp	WORD PTR [di+2+VBE_MODE_INFO_HEIGHT], si
jne	1f
mov	WORD PTR [vbe_selected_mode], dx
jmp	.vbe_choose_graphics_mode_after_loop

1:
# set vbe_best_mode if currently == 0xffff
cmp	WORD PTR [vbe_best_mode], 0xffff
jne	2f
movzx	eax, WORD PTR [di+2+VBE_MODE_INFO_WIDTH]
movzx	esi, WORD PTR [di+2+VBE_MODE_INFO_HEIGHT]
imul	eax, esi
mov	WORD PTR [vbe_best_mode], dx
mov	DWORD PTR [vbe_best_mode_mul], eax
jmp	.vbe_choose_graphics_mode_continue

2:
# set vbe_best_mode if width*height > vbe_best_mode_mul
movzx	eax, WORD PTR [di+2+VBE_MODE_INFO_WIDTH]
movzx	esi, WORD PTR [di+2+VBE_MODE_INFO_HEIGHT]
imul	eax, esi
cmp	eax, DWORD PTR [vbe_best_mode_mul]
jle	.vbe_choose_graphics_mode_continue
mov	WORD PTR [vbe_best_mode], dx
mov	DWORD PTR [vbe_best_mode_mul], eax

.vbe_choose_graphics_mode_continue:
inc	dx
add	di, 2 + VBE_INFO_SIZE
jmp	.vbe_choose_graphics_mode_loop

.vbe_choose_graphics_mode_after_loop:

cmp	WORD PTR [vbe_selected_mode], 0xffff
jne	1f
cmp	WORD PTR [vbe_best_mode], 0xffff
jne	0f
xor	ax, ax
jmp	.vbe_choose_graphics_mode_end

0:
mov	dx, WORD PTR [vbe_best_mode]
mov	WORD PTR [vbe_selected_mode], dx

1:
mov	dx, WORD PTR [vbe_selected_mode]
mov	ax, dx
imul	ax, 2 + VBE_INFO_SIZE
lea	di, [vbe_modes]
add	di, ax
mov	cx, WORD PTR [di]
or	cx, 0x4000 # set "use LFB" flag in mode
call	vbe_set_mode
mov	ax, 1

.vbe_choose_graphics_mode_end:
pop	di
pop	esi
ret

.global vbe_choose_mode
vbe_choose_mode:
push	di
push	esi
cmp	WORD PTR [vbe_num_modes], 0
je	.vbe_choose_mode_end

cmp	WORD PTR [vbe_preferred_text_rows], 0
jne	0f
cmp	WORD PTR [vbe_preferred_text_cols], 0
jne	0f
call	vbe_choose_graphics_mode
cmp	ax, 0
jne	.vbe_choose_mode_end

0:
xor	dx, dx
lea	di, [vbe_modes]

.vbe_choose_mode_loop:
cmp	dx, WORD PTR [vbe_num_modes]
je	.vbe_choose_mode_after_loop

# continue if not text
cmp	BYTE PTR [di+2+VBE_MODE_INFO_MEMORY_MODEL], MM_TEXT
jne	.vbe_choose_mode_continue

# continue if width == 0 || height == 0
cmp	WORD PTR [di+2+VBE_MODE_INFO_WIDTH], 0
je	.vbe_choose_mode_continue
cmp	WORD PTR [di+2+VBE_MODE_INFO_HEIGHT], 0
je	.vbe_choose_mode_continue

# break if width == vbe_preferred_text_rows && height == vbe_preferred_text_cols
mov	si, WORD PTR [vbe_preferred_text_rows]
cmp	WORD PTR [di+2+VBE_MODE_INFO_WIDTH], si
jne	1f
mov	si, WORD PTR [vbe_preferred_text_cols]
cmp	WORD PTR [di+2+VBE_MODE_INFO_HEIGHT], si
jne	1f
mov	WORD PTR [vbe_selected_mode], dx
jmp	.vbe_choose_mode_after_loop

1:
# set vbe_best_mode if currently == 0xffff
cmp	WORD PTR [vbe_best_mode], 0xffff
jne	2f
movzx	eax, WORD PTR [di+2+VBE_MODE_INFO_WIDTH]
movzx	esi, WORD PTR [di+2+VBE_MODE_INFO_HEIGHT]
imul	eax, esi
mov	WORD PTR [vbe_best_mode], dx
mov	DWORD PTR [vbe_best_mode_mul], eax
jmp	.vbe_choose_mode_continue

2:
# set vbe_best_mode if width*height > vbe_best_mode_mul
movzx	eax, WORD PTR [di+2+VBE_MODE_INFO_WIDTH]
movzx	esi, WORD PTR [di+2+VBE_MODE_INFO_HEIGHT]
imul	eax, esi
cmp	eax, DWORD PTR [vbe_best_mode_mul]
jle	.vbe_choose_mode_continue
mov	WORD PTR [vbe_best_mode], dx
mov	DWORD PTR [vbe_best_mode_mul], eax

.vbe_choose_mode_continue:
inc	dx
add	di, 2 + VBE_INFO_SIZE
jmp	.vbe_choose_mode_loop

.vbe_choose_mode_after_loop:
cmp	WORD PTR [vbe_selected_mode], 0xffff
jne	1f
cmp	WORD PTR [vbe_best_mode], 0xffff
je	.vbe_choose_mode_end # FIXME - should return error if this happens
mov	dx, WORD PTR [vbe_best_mode]
mov	WORD PTR [vbe_selected_mode], dx

1:
mov	dx, WORD PTR [vbe_selected_mode]
mov	ax, dx
imul	ax, 2 + VBE_INFO_SIZE
lea	di, [vbe_modes]
add	di, ax
mov	cx, WORD PTR [di]
call	vbe_set_mode

.vbe_choose_mode_end:
pop	esi
pop	di
ret

parse_integer: # uint16_t parse_integer(char* s, uint16_t len)
push	bx

xor	ax, ax
and	ecx, 0xffff
xor	bh, bh

.parse_integer_loop:
cmp	dx, 0
je	.parse_integer_end

cmp	BYTE PTR es:[ecx], '0'
jl	.parse_integer_end

cmp	BYTE PTR es:[ecx], '9'
jg	.parse_integer_end

imul	ax, 10
mov	bl, BYTE PTR es:[ecx]
sub	bl, '0'
add	ax, bx

inc	cx
dec	dx
jmp	.parse_integer_loop

.parse_integer_end:
pop	bx
ret

parse_text_option: # void parse_text_option(char* cmd, uint16_t len)
push	bx

# bx is x, eax is i

and	ecx, 0xffff
mov	eax, 0

.parse_text_option_loop:
cmp	ax, dx
jge	.parse_text_option_end

cmp	BYTE PTR es:[ecx+eax], 'x'
jne	.parse_text_option_continue
mov	bx, ax
jmp	1f

.parse_text_option_continue:
inc	ax
jmp	.parse_text_option_loop

1:
push	cx
push	dx
mov	dx, bx
call	parse_integer
mov	WORD PTR [vbe_preferred_text_rows], ax
pop	dx
pop	cx

add	cx, bx
inc	cx
sub	dx, bx
dec	dx
call	parse_integer
mov	WORD PTR [vbe_preferred_text_cols], ax

.parse_text_option_end:
pop	bx
ret

parse_video_option: # void parse_video_option(char* cmd, uint16_t len)
push	bx

# bx is x, eax is i

and	ecx, 0xffff
mov	eax, 0

.parse_video_option_loop:
cmp	ax, dx
jge	.parse_video_option_end

cmp	BYTE PTR es:[ecx+eax], 'x'
jne	.parse_video_option_continue
mov	bx, ax
jmp	1f

.parse_video_option_continue:
inc	ax
jmp	.parse_video_option_loop

1:
push	cx
push	dx
mov	dx, bx
call	parse_integer
mov	WORD PTR [vbe_preferred_video_width], ax
pop	dx
pop	cx

add	cx, bx
inc	cx
sub	dx, bx
dec	dx
call	parse_integer
mov	WORD PTR [vbe_preferred_video_height], ax

.parse_video_option_end:
pop	bx
ret

parse_option: # void parse_option(char* cmd, uint16_t len)
and	ecx, 0xffff

cmp	dx, 5
jl	.parse_option_video
cmp	DWORD PTR es:[ecx], 0x74786574 # 'text'
jne	.parse_option_video
cmp	BYTE PTR es:[ecx+4], '='
jne	.parse_option_video
add	ecx, 5
sub	dx, 5
call	parse_text_option
jmp	.parse_option_end

.parse_option_video:
cmp	dx, 6
jl	.parse_option_end
cmp	DWORD PTR es:[ecx], 0x65646976 # 'vide'
jne	.parse_option_end
cmp	WORD PTR es:[ecx+4], 0x3d6f # 'o='
jne	.parse_option_end
add	ecx, 6
sub	dx, 6
call	parse_video_option
jmp	.parse_option_end

.parse_option_end:
ret

.global parse_cmdline
parse_cmdline:
push	di
push	si
push	bx

# translate cmdline to segment (es) and offset (bx)
mov	eax, DWORD PTR [cmdline] # FIXME
mov	eax, DWORD PTR [eax]
mov	bx, ax
and	bx, 0xf
shr	eax, 4
mov	es, ax

xor	di, di # start
xor	si, si # i

.parse_cmdline_loop:

cmp	BYTE PTR es:[bx+si], 0
jne	1f
mov	cx, bx
mov	dx, si
call	parse_option
jmp	.parse_cmdline_end

1:
cmp	BYTE PTR es:[bx+si], ' '
jne	2f
mov	cx, bx
mov	dx, si
call	parse_option
add	bx, si
inc	bx
xor	si, si
jmp	.parse_cmdline_loop

2:
inc	si
jmp	.parse_cmdline_loop

.parse_cmdline_end:
push	ds
pop	es
pop	bx
pop	si
pop	di
ret

.section .rdata
kernel_version:
.asciz	MOXNOX_VERSION_STRING

.section .data
.global e820_entries
e820_entries:
.space	E820_SIZE * E820_COUNT

.global e820_count
e820_count:
.long	0

.global cmdline
cmdline:
.long	.cmd_line_ptr - start + 0x200

.section .vbe
.global vbe_info
vbe_info:
.space	512

.global vbe_edid
vbe_edid:
.space	128

.global vbe_edid_valid
vbe_edid_valid:
.byte	0

.global vbe_modes
vbe_modes:
.space	(2 + VBE_INFO_SIZE) * MAX_VBE_MODES
.space	256 - VBE_INFO_SIZE # padding

vbe_num_modes:
.short	0

.global vbe_selected_mode
vbe_selected_mode:
.short	0xffff

vbe_preferred_text_rows:
.short	0

vbe_preferred_text_cols:
.short	0

vbe_preferred_video_width:
.short	0

vbe_preferred_video_height:
.short	0

vbe_best_mode:
.short	0xffff

vbe_best_mode_mul:
.long	0
