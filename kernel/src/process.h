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
    bool add_thread(uintptr_t entry_point, uintptr_t param1, uintptr_t param2, uintptr_t stack);
    bool handle_page_fault(uintptr_t address, uint64_t error_code);
    bool allocate_cr3();
    bool copy_from(void* ks_addr, const void* us_addr, size_t size);
    bool copy_to(void* us_addr, const void* ks_addr, size_t size);
    void terminate();
    void resume();
    handle_t add_handle(object& obj, uint32_t access_mask);
    NTSTATUS allocate_vm(void** base_address, uintptr_t zero_bits, size_t* region_size,
                         uint32_t allocation_type, enum mapping_type protect);
    NTSTATUS free_vm(void** base_address, size_t* region_size, uint32_t free_type);
    void free_mappings();

    bool signalled = false;
    std::optional<std::reference_wrapper<section>> img;
    spinlock mappings_lock;
    list_entry mappings = { &mappings, &mappings };
    uint64_t cr3;
    spinlock threads_lock;
    list_entry threads = { &threads, &threads };
    enum nt_status exit_status = nt_status::PENDING;
    std::atomic<unsigned int> suspend_count;
    spinlock handle_lock;
    unsigned int no_handles = 0;
    unsigned int handle_arr_pages = 0;
    handle_obj* handle_arr = nullptr;
};

static_assert(is_sync_object<process>);

static_assert(offsetof(process, cr3) == PROCESS_CR3);

enum class proc_map_type {
    section,
    reserved,
    committed
};

struct page_info {
    uint64_t pa;
    enum mapping_type protect;
};

class mapping {
public:
    mapping() = default;
    mapping(section& sect);

    ~mapping();

    list_entry le;
    enum proc_map_type type;
    uint64_t address;
    uint32_t size;
    section* sect = nullptr;
};

class mapping_committed : public mapping {
public:
    spinlock lock;
    page_info pages[0];

    bool map(uint32_t offset, uint64_t va);
    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
    bool copy_to(uint32_t offset, const void* ks_addr, uint32_t size);
};
