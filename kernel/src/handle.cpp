#include "kernel.h"
#include "objects.h"
#include "spinlock.h"

using namespace std;

struct handle_obj {
    object* obj;
    uint32_t access_mask;
};

static unsigned int no_handles = 0;
static handle_obj* const handle_arr = (handle_obj*)0xfffffa2000000000;
static size_t handle_arr_pages = 0;
static spinlock handle_spinlock;

struct name_entry {
    list_entry le;
    object* obj;
    size_t name_len;
    char name[1];
};

static spinlock name_lock;
static list_entry name_list = { &name_list, &name_list };

handle_t add_handle(object& obj, uint32_t access_mask) {
    handle_t h;

    {
        lock_guard lock(handle_spinlock);

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

        h = no_handles;

        auto& ho = handle_arr[h - 1];
        ho.obj = &obj;
        ho.access_mask = access_mask;
    }

    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });

    return h;
}

void close_handle(handle_t h) {
    object* obj;

    {
        lock_guard lock(handle_spinlock);

        auto& ho = handle_arr[h - 1];
        obj = ho.obj;

        ho.obj = nullptr;
    }

    visit_obj(*obj, [](auto&& o) {
        o.release();
    });
}

optional<reference_wrapper<object>> deref_handle(handle_t h) {
    lock_guard lock(handle_spinlock);

    if (h == 0 || h > no_handles)
        return nullopt;

    auto& ho = handle_arr[h - 1];

    if (!ho.obj)
        return nullopt;

    visit_obj(*ho.obj, [](auto&& o) {
        o.acquire(); // needs to be within spinlock
    });

    return *ho.obj;
}

bool name_object(handle_t h, string_view name) {
    if (name.empty()) {
        // FIXME - exception
        print("Invalid name\n");
        return false;
    }

    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        print("No such handle\n");
        return false;
    }

    auto& obj = objrw->get();

    auto ne = (name_entry*)malloc(offsetof(name_entry, name) + name.size());
    if (!ne) {
        // FIXME - exception
        print("Out of memory\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return false;
    }

    optional<lock_guard<spinlock>> lock(name_lock);

    if (obj.named) {
        lock.reset();
        // FIXME - exception
        print("Object already has name\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        free(ne);

        return false;
    }

    auto le = name_list.next;
    while (le != &name_list) {
        auto& ne2 = containing_record<name_entry>(le);

        if (ne2.name_len == name.length()) {
            auto ne2sv = string_view(ne2.name, ne2.name_len);

            if (ne2sv == name) {
                lock.reset();
                // FIXME - exception
                print("Name collision\n");

                visit_obj(obj, [](auto&& o) {
                    o.release();
                });

                free(ne);

                return false;
            }
        }

        le = le->next;
    }

    obj.named = true;
    ne->obj = &obj;
    ne->name_len = name.length();
    memcpy(ne->name, name.data(), name.length());
    insert_tail_list(name_list, ne->le);

    return true;
}
