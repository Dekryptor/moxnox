#include "kernel.h"
#include "objects.h"

using namespace std;

struct handle_obj {
    object* obj;
    uint32_t access_mask;
};

static unsigned int no_handles = 0;
static handle_obj* const handle_arr = (handle_obj*)0xfffffa2000000000;
static size_t handle_arr_pages = 0;

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

        if (!map_page(pa, (uintptr_t)handle_arr + (handle_arr_pages << PAGE_SHIFT), mapping_type::read_write)) {
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

void close_handle(handle_t h) {
    object* obj;

    {
        // FIXME - spinlock

        auto& ho = handle_arr[h - 1];
        obj = ho.obj;

        ho.obj = nullptr;
    }

    visit_obj(*obj, [](auto&& o) {
        o.release();
    });
}

optional<reference_wrapper<object>> deref_handle(handle_t h) {
    // FIXME - spinlock

    if (h == 0 || h > no_handles)
        return nullopt;

    auto& ho = handle_arr[h - 1];

    if (!ho.obj)
        return nullopt;

    visit_obj(*ho.obj, [](auto&& o) {
        o.acquire();
    });

    return *ho.obj;
}
