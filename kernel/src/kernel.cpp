#include "../../bootloader/src/loaderblock.h"
#include "kernel.h"

extern uint64_t __CTOR_LIST__;

static void call_global_constructors() {
    auto v = &__CTOR_LIST__;

    if (*v != 0xffffffffffffffff)
        return;

    v++;

    while (*v != 0) {
        auto func = reinterpret_cast<void (*)()>(*v);

        func();

        v++;
    }
}

extern "C"
__attribute((sysv_abi))
void startup(loader_block* lb) {
    // FIXME - should restart if anything fails before we can show error message

    if (!init_memory(*lb))
        goto hang;

    lb = nullptr; // no longer valid

    call_global_constructors();

    // FIXME - map video memory at b8000

    // FIXME - interrupts
    // FIXME - scheduler

hang:
    __asm("cli; hlt");
}
