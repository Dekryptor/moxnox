#include "kernel.h"
#include "thread.h"

struct handle_obj {
    object* obj;
    uint32_t access_mask;
};

static unsigned int no_handles = 0;
static handle_obj* const handle_arr = (handle_obj*)0xfffffa2000000000;
static size_t handle_arr_pages = 0;

// FIXME - concept to make sure func is invocable
template<typename T>
static void visit_obj(object& obj, const T& func) {
    switch (obj.type) {
        case object_type::thread:
            func(static_cast<thread&>(obj));
        break;
    }
}

handle_t add_handle(object& obj, uint32_t access_mask) {
    // FIXME - spinlock

    size_t needed_pages = bytes_to_pages((no_handles + 1) * sizeof(handle_obj));

    if (needed_pages > handle_arr_pages) {
        auto pa = allocate_page();
        if (!pa) {
            // FIXME - should be exception
            print("allocate_page failed\n");
            return 0;
        }

        if (!map_page(pa, (uintptr_t)handle_arr + (handle_arr_pages << PAGE_SHIFT))) {
            // FIXME - exception
            print("map_page failed\n");
            free_page(pa);
            return 0;
        }

        handle_arr_pages++;
    }

    no_handles++;

    handle_t h = no_handles;

    auto& ho = handle_arr[h - 1];
    ho.obj = &obj;
    ho.access_mask = access_mask;

    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });

    return h;
}
