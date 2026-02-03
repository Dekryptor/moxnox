module;

#include <optional>
#include <mutex>
#include <string_view>

module handle;

import print;
import visit;
import mem;
import kernel_time_page;

using namespace std;

nt_status name_object(object& obj, string_view name, bool persistent) {
    if (name.empty() || name.size() > MAX_NAME_LEN)
        return nt_status::OBJECT_NAME_INVALID;

    auto ne = (name_entry*)malloc(offsetof(name_entry, name) + name.size());
    if (!ne) {
        print("Out of memory\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    optional<lock_guard<spinlock>> lock(name_lock);

    if (obj.name) {
        lock.reset();
        print("Object already has name\n");
        free(ne);
        return nt_status::INTERNAL_ERROR; // FIXME?
    }

    for (auto le = name_list.next; le != &name_list; le = le->next) {
        auto& ne2 = containing_record<name_entry>(le);

        if (ne2.name_len == name.length()) {
            auto ne2sv = string_view(ne2.name, ne2.name_len);

            if (ne2sv == name) {
                lock.reset();
                free(ne);
                return nt_status::OBJECT_NAME_COLLISION;
            }
        }
    }

    obj.name = ne;
    obj.persistent = persistent;
    ne->obj = &obj;

    if (is_list_empty(name_list))
        ne->index = 2;
    else
        ne->index = containing_record<name_entry>(name_list.prev).index + 1;

    ne->creation_time = cur_time;
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

void remove_object_name(object& obj) {
    name_entry* ne;

    {
        lock_guard lock(name_lock);

        if (!obj.name)
            return;

        remove_entry_list(obj.name->le);

        ne = obj.name;
        obj.name = nullptr;
    }

    free(ne);
    object_release(obj);
}

void handle_closed(object& obj) {
    auto new_handle_count = --obj.handle_count;

    // remove name if last handle closed

    if (new_handle_count == 0)
        object_last_handle_closed(obj);

    if (obj.name && !obj.persistent && new_handle_count == 0)
        remove_object_name(obj);
}
