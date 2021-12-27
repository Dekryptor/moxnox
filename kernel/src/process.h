#pragma once

#include "spinlock.h"
#include "unique_handle.h"

class section;

class process : public object {
public:
    process() : object(object_type::process) { }
    ~process();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool map_section(section& sect, uint64_t address);
    bool add_thread(uintptr_t entry_point);

    bool signalled = false;
    std::optional<std::reference_wrapper<section>> img;
    spinlock mappings_lock;
    list_entry mappings = { &mappings, &mappings };
};

static_assert(is_sync_object<process>);

class mapping {
public:
    mapping(section& sect);
    ~mapping();

    list_entry le;
    uint64_t address;
    uint32_t size;
    section& sect;
};
