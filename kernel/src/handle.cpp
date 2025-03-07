#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "handle.h"

import print;

using namespace std;

struct name_entry {
    list_entry le;
    object* obj;
    size_t name_len;
    char name[1];
};

static spinlock name_lock;
static list_entry name_list = { &name_list, &name_list };

optional<reference_wrapper<object>> deref_handle(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h);
}

object_ref deref_handle2(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle2(h);
}

bool name_object(object& obj, string_view name, bool persistent) {
    if (name.empty()) {
        // FIXME - exception
        print("Invalid name\n");
        return false;
    }

    auto ne = (name_entry*)malloc(offsetof(name_entry, name) + name.size());
    if (!ne) {
        // FIXME - exception
        print("Out of memory\n");
        return false;
    }

    optional<lock_guard<spinlock>> lock(name_lock);

    if (obj.named) {
        lock.reset();
        // FIXME - exception
        print("Object already has name\n");
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
                free(ne);
                return false;
            }
        }

        le = le->next;
    }

    obj.named = true;
    obj.persistent = persistent;
    ne->obj = &obj;
    ne->name_len = name.length();
    memcpy(ne->name, name.data(), name.length());
    insert_tail_list(name_list, ne->le);

    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });

    return true;
}

object_ref open_object(string_view name) {
    if (name.empty()) {
        print("Invalid name\n");
        return {};
    }

    object* obj = nullptr;

    {
        lock_guard lock(name_lock);

        auto le = name_list.next;
        while (le != &name_list) {
            auto& ne = containing_record<name_entry>(le);

            if (ne.name_len == name.length()) {
                auto nesv = string_view(ne.name, ne.name_len);

                if (nesv == name) {
                    visit_obj(*ne.obj, [](auto&& o) {
                        o.acquire();
                    });

                    obj = ne.obj;
                    break;
                }
            }

            le = le->next;
        }
    }

    if (!obj) {
        print("Not found\n");
        return {};
    }

    return *obj;
}

void write(object_ref& h, span<const uint8_t> s) {
    if (!h) {
        // FIXME - exception
        print("Object not set\n");
        return;
    }

    auto& obj = h.get();
    bool can_write = false;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            can_write = true;
    });

    if (!can_write) {
        // FIXME - exception
        print("Not a writable object\n");
        return;
    }

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            o.write(s);
    });
}

void write(object_ref& h, string_view s) {
    write(h, span((uint8_t*)s.data(), s.size()));
}

void handle_closed(object& obj) {
    auto new_handle_count = --obj.handle_count;

    // remove name if last handle closed

    if (obj.named && !obj.persistent && new_handle_count == 0) {
        name_entry* ne = nullptr;

        {
            lock_guard lock(name_lock);

            auto le = name_list.next;
            while (le != &name_list) {
                auto& ne2 = containing_record<name_entry>(le);

                if (ne2.obj == &obj) {
                    ne = &ne2;
                    remove_entry_list(ne2.le);
                    break;
                }

                le = le->next;
            }
        }

        if (ne) {
            free(ne);

            visit_obj(obj, [](auto&& o) {
                o.release();
            });
        }
    }
}
