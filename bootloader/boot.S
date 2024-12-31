#include <version.h>

.intel_syntax noprefix
.code16

.equ E820_COUNT, 128
.equ E820_SIZE, 20

.section ".boot", "a"

.space  0x1f1

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
jne	.vbe_get_info_end

mov	eax, DWORD PTR [vbe_info]
cmp	eax, 0x41534556
jne	.vbe_get_info_end

mov	BYTE PTR [vbe_info_valid], 1

.vbe_get_info_end:
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

.global vbe_info
vbe_info:
.space 512

.global vbe_info_valid
vbe_info_valid:
.byte 0

.global cmdline
cmdline:
.long	.cmd_line_ptr - start + 0x200
