.intel_syntax noprefix
.code16

.section ".boot", "a"

.space  0x1f1

.byte	setup_sects - 1
.short	1 # root_flags (ROOT_RDONLY)
.long	0 # FIXME - syssize (adb40)
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
.short	0 # FIXME - kernel_version (38c0)
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
mov	di, OFFSET __bss_start
mov	cx, OFFSET _end + 3
xor	eax, eax
sub	cx, di
shr	cx, 2
rep stosd

push	ds
push	OFFSET 1f
lret

1:
jmp real_mode_main
