.section .rdata

.globl _payload_size
_payload_size:
.quad payload_end - _payload_data

.section .payload

.globl _payload_data
_payload_data:
.incbin "kernel.exe"
payload_end:
