#pragma once

#include <optional>
#include <functional>
#include <string_view>
#include <stdint.h>

import nt;
import objects;
import object_ref;

using handle_t = uint64_t;

object_ref deref_handle(handle_t h);
object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask);
nt_status name_object(object& obj, std::string_view name, bool persistent = true);
nt_status open_object(std::string_view name, object_ref& obj);
void handle_closed(object& obj);
void remove_object_name(object& obj);
