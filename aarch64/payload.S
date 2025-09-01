.section .rdata

.globl payload_size
payload_size:
.quad payload_end - payload_data

.section .payload

.globl payload_data
payload_data:
.incbin "kernel.exe"
payload_end:
