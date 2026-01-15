#pragma once

#include <stdint.h>
#include <stddef.h>
#include <atomic>
#include "context_offsets.h"

import nt;
import list;
import object_ref;
import objects;

enum class thread_status {
    runnable,
    running,
    waiting,
    waiting_multiple,
    dead,
    dying
};

extern "C++" class process;

enum class waiter_object_type {
    thread,
    multiple
};

class waiter_object {
public:
    waiter_object(enum waiter_object_type type) : type(type) { }

    list_entry wait_le;
    enum waiter_object_type type;
};

struct multiple_wait_block;

class waiter_object_multiple : public waiter_object {
public:
    waiter_object_multiple() : waiter_object(waiter_object_type::multiple) { }

    multiple_wait_block* block;
    bool done = false;
};
