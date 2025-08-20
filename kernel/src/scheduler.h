#pragma once

#include <nt.h>
#include <span>
#include <optional>

import list;

class object;

// scheduler.cpp
extern list_entry thread_list, thread_list_dead;
extern "C" void scheduler();
nt_status wait_for_object(object& obj, bool interruptible,
                          std::optional<uint64_t> timeout = std::nullopt);
nt_status wait_for_objects(std::span<object*> objects, bool wait_any,
                           std::optional<uint64_t> timeout = std::nullopt);
bool init_cpu_block(unsigned int cpu_no);
void disable_preempt();
void enable_preempt();
unsigned int wake_up_waiters(list_entry& waiters);
void set_current_thread_waiting();
