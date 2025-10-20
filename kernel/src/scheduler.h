#pragma once

#include <optional>

import nt;
import list;

class object;

// scheduler.cpp
extern list_entry thread_list;
nt_status wait_for_object(object& obj, bool interruptible,
                          std::optional<uint64_t> timeout = std::nullopt);
unsigned int wake_up_waiters(list_entry& waiters);
void set_current_thread_waiting();
