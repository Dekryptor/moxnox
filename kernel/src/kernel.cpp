#include "../../bootloader/src/loaderblock.h"
#include "kernel.h"

extern "C"
__attribute((sysv_abi))
void startup(loader_block* lb) {
    // FIXME - should restart if anything fails before we can show error message

    if (!init_memory(*lb))
        goto hang;

    lb = nullptr; // no longer valid

    // FIXME - map video memory at b8000

    // FIXME - calling global constructors?

    // FIXME - interrupts
    // FIXME - scheduler

hang:
    __asm("cli; hlt");
}
