// FIXME - DWARF information

.global scheduler_restore
scheduler_restore:
    b scheduler_restore // FIXME

.global get_cpu_block
get_cpu_block:
    mov x0, x18
    ret
get_cpu_block_end:
