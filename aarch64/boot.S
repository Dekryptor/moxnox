.section ".boot", "a"

ccmp	x18, #0, #0xd, pl // instruction that happens to start with "MZ"
b	start
.quad	0 // text_offset
.quad	0 // image_size (FIXME)
.quad	0xa // flags (place anywhere, page size = 4K, little-endian)
.quad	0 // res2
.quad	0 // res3
.quad	0 // res4
.long	0x644d5241 // magic
.long	0x40 // PE header offset (FIXME)
