module;

#include <string_view>
#include <stdint.h>

export module handle;

import nt;
import objects;
import object_ref;
import spinlock;
import list;

using namespace std;

export using handle_t = uint64_t;

export const size_t MAX_NAME_LEN = 256;

export spinlock name_lock;
export list_entry name_list = { &name_list, &name_list };

export nt_status name_object(object& obj, string_view name, bool persistent = true);
export nt_status open_object(string_view name, object_ref& obj);
export void remove_object_name(object& obj);
export void handle_closed(object& obj);
