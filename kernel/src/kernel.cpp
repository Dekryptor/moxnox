#include "../../bootloader/src/loaderblock.h"
#include "kernel.h"
#include "unique_handle.h"
#include <string_view>

using namespace std;

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

static void timer_test() {
    unique_handle h;

    if (!create_timer(h)) {
        print("create_timer failed\n");
        return;
    }

    print("Waiting...\n");

    set_timer(h.get(), 5000, false);

    wait_for_object(h.get());

    print("Timer fired.\n");
}

void main_thread(void*) {
    init_keyboard();

    thread_test(); // FIXME

    init_smp();

    protect_kernel_pages();

    timer_test(); // FIXME

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

    if (!init_vablock())
        goto hang;

    if (!init_console())
        goto hang;

    if (!init_interrupts())
        goto hang;

    if (!init_cpu_block(true, current_cpu_id()))
        goto hang;


    if (!add_kernel_thread(main_thread))
        goto hang;

    // FIXME - C++ exceptions

    init_timer(); // leaves interrupts disabled

    scheduler(); // doesn't return

hang:
    while (1) { }
}
