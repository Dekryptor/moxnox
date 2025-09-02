.section ".boot", "a"

ccmp	x18, #0, #0xd, pl // instruction that happens to start with "MZ"
b	start_asm
.quad	0 // text_offset
.quad	0 // image_size (FIXME)
.quad	0xa // flags (place anywhere, page size = 4K, little-endian)
.quad	0 // res2
.quad	0 // res3
.quad	0 // res4
.long	0x644d5241 // magic
.long	0x40 // PE header offset (FIXME)

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
