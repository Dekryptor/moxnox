#include "kernel.h"
#include "spinlock.h"
import objects;
#include "process.h"

import print;
import list;
import visit;

using namespace std;

struct name_entry {
    list_entry le;
    object* obj;
    size_t name_len;
    char name[1];
};

static spinlock name_lock;
static list_entry name_list = { &name_list, &name_list };

object_ref deref_handle(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h);
}

object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h, access_mask);
}

void object_ref::reset() {
    if (rw) {
        object_release(rw->get());
    }
}

nt_status name_object(object& obj, string_view name, bool persistent) {
    if (name.empty())
        return nt_status::OBJECT_NAME_INVALID;

    auto ne = (name_entry*)malloc(offsetof(name_entry, name) + name.size());
    if (!ne) {
        print("Out of memory\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    optional<lock_guard<spinlock>> lock(name_lock);

    if (obj.named) {
        lock.reset();
        print("Object already has name\n");
        free(ne);
        return nt_status::INTERNAL_ERROR; // FIXME?
    }

    auto le = name_list.next;
    while (le != &name_list) {
        auto& ne2 = containing_record<name_entry>(le);

        if (ne2.name_len == name.length()) {
            auto ne2sv = string_view(ne2.name, ne2.name_len);

            if (ne2sv == name) {
                lock.reset();
                free(ne);
                return nt_status::OBJECT_NAME_COLLISION;
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

    object_acquire(obj);

    return nt_status::SUCCESS;
}

nt_status open_object(string_view name, object_ref& obj) {
    if (name.empty()) {
        obj = {};
        return nt_status::OBJECT_NAME_INVALID;
    }

    {
        lock_guard lock(name_lock);

        auto le = name_list.next;
        while (le != &name_list) {
            auto& ne = containing_record<name_entry>(le);

            if (ne.name_len == name.length()) {
                auto nesv = string_view(ne.name, ne.name_len);

                if (nesv == name) {
                    object_acquire(*ne.obj);

                    obj = *ne.obj;
                    return nt_status::SUCCESS;
                }
            }

            le = le->next;
        }
    }

    obj = {};
    return nt_status::OBJECT_NAME_NOT_FOUND;
}

void handle_closed(object& obj, bool server_handle) {
    auto new_handle_count = --obj.handle_count;

    // remove name if last handle closed

    if (server_handle)
        object_server_handle_closed(obj);

    if (new_handle_count == 0)
        object_last_handle_closed(obj);

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
            object_release(obj);
        }
    }
}
