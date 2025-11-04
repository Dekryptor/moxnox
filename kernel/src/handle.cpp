module;

#include <optional>
#include <functional>
#include <string_view>
#include <stdint.h>

export module handle;

import nt;
import objects;
import object_ref;

using namespace std;

export {

using handle_t = uint64_t;

extern "C++" object_ref deref_handle(handle_t h);
extern "C++" object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask);
extern "C++" nt_status name_object(object& obj, string_view name, bool persistent = true);
extern "C++" nt_status open_object(string_view name, object_ref& obj);
extern "C++" void handle_closed(object& obj);
extern "C++" void remove_object_name(object& obj);

}
