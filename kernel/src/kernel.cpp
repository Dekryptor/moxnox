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

static void thread_test() {
    handle_t h;

    if (!add_kernel_thread([](void*) {
        print("Hello from second thread\n");
    }, nullptr, &h)) {
        print("add_kernel_thread failed\n");
    } else {
        print("h = ");
        print_hex(h);
        print("\n");

        wait_for_object(h);
        print("second thread finished\n");

        close_handle(h);
    }
}

void main_thread(void*) {
    thread_test(); // FIXME

    init_smp();

    protect_kernel_pages();

    while (1) { }
}

extern "C"
void startup(loader_block* lb) {
    // FIXME - should restart if anything fails before we can show error message

    {
        volatile int wait = 1;
        while (wait) { }
    }

    if (!init_memory(*lb))
        goto hang;

    lb = nullptr; // no longer valid

    call_global_constructors();

    if (!init_console())
        goto hang;

    if (!init_interrupts())
        goto hang;

    if (!init_cpu_block())
        goto hang;

    if (!add_kernel_thread(main_thread))
        goto hang;

    // FIXME - C++ exceptions

    init_timer(); // leaves interrupts disabled

    scheduler(); // doesn't return

hang:
    while (1) { }
}
