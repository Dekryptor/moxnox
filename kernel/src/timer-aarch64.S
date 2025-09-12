// FIXME - DWARF information

.global scheduler_restore
scheduler_restore:
    b scheduler_restore // FIXME

.global acquire_sync_lock
acquire_sync_lock:
    // FIXME
    ret

.global release_sync_lock
release_sync_lock: # clobbers rax
    // FIXME
    ret

.global get_cpu_block
get_cpu_block:
    mov x0, x18
    ret
get_cpu_block_end:
