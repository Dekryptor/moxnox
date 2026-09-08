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

export using wait_type_t = uint8_t;

export const wait_type_t WAIT_TYPE_READING = 1;
export const wait_type_t WAIT_TYPE_WRITING = 2;

export list_entry thread_list = { &thread_list, &thread_list };

export atomic<uint64_t> active_cpus = 0;

export nt_status wait_for_object(object& obj, bool interruptible,
                                 optional<uint64_t> timeout = nullopt);
export unsigned int wake_up_waiters(list_entry& waiters, wait_type_t wait_type);
export uintptr_t init_cpu_block(unsigned int cpu_no);
export nt_status wait_for_objects(span<object*> objects, bool wait_any,
                                  bool interruptible,
                                  optional<uint64_t> timeout = nullopt);
export void set_current_thread_waiting(bool interruptible);
export nt_status setup_user_apc(uint64_t return_value);
export nt_status wait_no_object(bool interruptible, optional<uint64_t> timeout);

export extern "C" {
    list_entry thread_list_timeout = { &thread_list_timeout, &thread_list_timeout };
}
