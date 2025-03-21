#pragma once

#include "spinlock.h"
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
    bool handle_page_fault(uintptr_t address, uint64_t error_code);
    bool allocate_cr3();
    bool allocate_peb(uintptr_t base_address, HANDLE stdinh, HANDLE stdouth, HANDLE stderrh,
                      HANDLE curdirh, std::string_view curdir, std::string_view fn);
    bool copy_from(void* ks_addr, const void* us_addr, size_t size);
    bool copy_to(void* us_addr, const void* ks_addr, size_t size);
    void terminate();
    void resume();
    nt_status add_handle(object& obj, uint32_t access_mask, bool allow_shared,
                        handle_t& h);
    NTSTATUS allocate_vm(void** base_address, uintptr_t zero_bits, size_t* region_size,
                         uint32_t allocation_type, enum mapping_type protect);
    NTSTATUS free_vm(void** base_address, size_t* region_size, uint32_t free_type);
    std::optional<std::reference_wrapper<object>> deref_handle(handle_t h);
    object_ref deref_handle2(handle_t h);
    NTSTATUS protect_vm(void** base_address, size_t* region_size, enum mapping_type prot,
                        enum mapping_type& old_prot);
    NTSTATUS close_handle(handle_t h);
    NTSTATUS map_view_of_section(section& sect, void** base_address,
                                 uintptr_t zero_bits, size_t commit_size, LARGE_INTEGER* section_offset,
                                 size_t& view_size, enum SECTION_INHERIT inherit_disposition,
                                 uint32_t allocation_type, uint32_t win32_protect);
    NTSTATUS unmap_view_of_section(void* base_address);
    NTSTATUS create_thread(std::optional<std::reference_wrapper<handle_t>> thread_handle,
                           ACCESS_MASK desired_access, const OBJECT_ATTRIBUTES* object_attributes,
                           CLIENT_ID& client_id, const CONTEXT& thread_context, const INITIAL_TEB& initial_teb,
                           bool create_suspended);
    NTSTATUS query_info(enum PROCESSINFOCLASS process_information_class, std::span<uint8_t> buf,
                        uint32_t* return_length);
    NTSTATUS read_vm(void* base_address, void* buffer, uint32_t buffer_size,
                     uint32_t* number_of_bytes_read);
    NTSTATUS write_vm(void* base_address, void* buffer, uint32_t buffer_size,
                      uint32_t* number_of_bytes_written);
    object_ref get_stderr();
    bool map_to_kernel_space(void* us, void* ks, unsigned int num_pages);
    void send_signal(uint64_t code);

    bool signalled = false;
    std::optional<std::reference_wrapper<section>> img;
    spinlock mappings_lock;
    list_entry mappings = { &mappings, &mappings };
    uint64_t cr3 = 0;
    spinlock threads_lock;
    list_entry threads = { &threads, &threads };
    enum nt_status exit_status = nt_status::PENDING;
    std::atomic<unsigned int> suspend_count;
    spinlock handle_lock;
    unsigned int no_handles = 0;
    unsigned int handle_arr_pages = 0;
    handle_obj* handle_arr = nullptr;
    PEB* peb = nullptr;
    RTL_USER_PROCESS_PARAMETERS* proc_params = nullptr;
    uintptr_t ntdll_addr, img_addr;
    list_entry waiters = { &waiters, &waiters };

private:
    void free_mappings();
    NTSTATUS unmap_proc_page(uint64_t va);
    bool populate_teb(thread& t);
    NTSTATUS change_proc_page_protection(uint64_t va, enum mapping_type prot);
};

static_assert(is_sync_object<process>);
static_assert(is_sync_object2<process>);

static_assert(offsetof(process, cr3) == PROCESS_CR3);

enum class proc_map_type {
    section,
    vm
};

class mapping {
public:
    list_entry le;
    enum proc_map_type type;
    uint64_t address;
    uint32_t size;
    spinlock lock;
};

class mapping_section : public mapping {
public:
    mapping_section(section& sect);
    ~mapping_section();

    section& sect;
    page_info pages[0];

    NTSTATUS protect(uint32_t offset, uint32_t page_count, enum mapping_type prot,
                     enum mapping_type& old_prot);
    bool map(uint32_t offset, uint64_t va);
    bool cow(uint32_t offset, uint64_t va);
    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
};

class mapping_vm : public mapping {
public:
    page_info pages[0];

    bool map(uint32_t offset, uint64_t va, std::optional<enum mapping_type> prot);
    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
    bool copy_to(uint32_t offset, const void* ks_addr, uint32_t size);
    NTSTATUS commit(uint32_t offset, uint32_t commit_size, enum mapping_type prot);
};
