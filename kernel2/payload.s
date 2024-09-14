.intel_syntax noprefix

.section .payload
.globl payload_data
payload_data:
.incbin "payload.exe"

payload_end:

.section .rdata
.globl payload_size
payload_size:
.quad payload_end - payload_data
