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

void test_pit();

extern "C"
__attribute((sysv_abi))
void startup(loader_block* lb) {
    // FIXME - should restart if anything fails before we can show error message

    if (!init_memory(*lb))
        goto hang;

    lb = nullptr; // no longer valid

    call_global_constructors();

    if (!init_console())
        goto hang;

    if (!init_interrupts())
        goto hang;

    test_pit();

    // FIXME - scheduler

hang:
    while (1) { }
}
