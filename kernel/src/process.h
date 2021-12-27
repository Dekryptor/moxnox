#pragma once

#include "spinlock.h"
#include "unique_handle.h"
#include "context_offsets.h"

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

    uintptr_t map_section(section& sect, uintptr_t address);
    bool add_thread(uintptr_t entry_point, uintptr_t stack);
    bool handle_page_fault(uintptr_t address, uint64_t error_code);
    bool allocate_cr3();

    bool signalled = false;
    std::optional<std::reference_wrapper<section>> img;
    spinlock mappings_lock;
    list_entry mappings = { &mappings, &mappings };
    uint64_t cr3;
};

static_assert(is_sync_object<process>);

static_assert(offsetof(process, cr3) == PROCESS_CR3);

class mapping {
public:
    mapping(section& sect);
    ~mapping();

    list_entry le;
    uint64_t address;
    uint32_t size;
    section& sect;
};
