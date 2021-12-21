#include "../../bootloader/src/loaderblock.h"
#include "kernel.h"
#include "unique_handle.h"
#include "context_offsets.h"
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

extern "C"
int atexit(void (*)(void)) {
    // needed for linking when there are global objects with destructors
    return 0;
}

static void thread_test() {
    auto ev = create_event(false);

    if (!ev) {
        print("create_event failed\n");
        return;
    }

    auto h = add_kernel_thread([](void* ctx) {
        auto evv = (handle_t)ctx;

        print("Hello from second thread\n");

        set_event(evv);
    }, (void*)ev.get());

    if (!h) {
        print("add_kernel_thread failed\n");
        return;
    }

    print("h = ");
    print_hex(h.get());
    print("\n");

    wait_for_object(ev.get());
    print("second thread finished\n");
}

#if 0
static void timer_test() {
    auto h = create_timer();
    if (!h) {
        print("create_timer failed\n");
        return;
    }

    print("Waiting...\n");

    set_timer(h.get(), 5000, false);

    wait_for_object(h.get());

    print("Timer fired.\n");
}
#endif

void main_thread(void*) {
    init_keyboard();

    init_smp();

    protect_kernel_pages();

    thread_test(); // FIXME
//     timer_test(); // FIXME

    while (1) {
        wait_for_object(keyboard_event.get());
        print("keyboard_event\n");
    }
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

    if (!init_cpu_block(true, current_cpu_id()))
        goto hang;

    if (!init_interrupts())
        goto hang;

    free_lower_half_mappings();

    if (!add_kernel_thread(main_thread))
        goto hang;

    // FIXME - C++ exceptions

    init_timer(); // leaves interrupts disabled

    __asm __volatile ( // doesn't return
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (GS_SCHEDULER_STACK)
    );

hang:
    while (1) { }
}
