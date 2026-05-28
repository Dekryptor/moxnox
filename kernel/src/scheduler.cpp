module;

#include <span>
#include <optional>
#include <atomic>
#include <stdint.h>

export module scheduler;

import nt;
import list;
import objects;

using namespace std;

export list_entry thread_list = { &thread_list, &thread_list };

export atomic<uint64_t> active_cpus = 0;

export nt_status wait_for_object(object& obj, bool interruptible,
                                 optional<uint64_t> timeout = nullopt);
export unsigned int wake_up_waiters(list_entry& waiters);
export uintptr_t init_cpu_block(unsigned int cpu_no);
export nt_status wait_for_objects(span<object*> objects, bool wait_any,
                                  optional<uint64_t> timeout = nullopt);
export void set_current_thread_waiting();
export nt_status setup_user_apc();
