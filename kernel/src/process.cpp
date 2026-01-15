module;

#include <string_view>
#include <span>
#include <atomic>
#include <optional>
#include "context_offsets.h"
#include "rangelock.h"

export module process;

import object_ref;
import nt;
import list;
import rbtree;
import mem;
import spinlock;
import handle;
import objects;

using namespace std;

extern "C++" class section;
extern "C++" class thread;

export extern "C++"
class process : public sync_object {
public:
    process();
    ~process();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    uintptr_t map_section(section& sect, uintptr_t address, optional<int64_t> offset,
                          enum mapping_type protect, bool cow, size_t* view_size);
#ifdef __aarch64__
    bool handle_page_fault(uintptr_t address, uint64_t esr_value);
    bool allocate_ttbr0();
#elifdef __x86_64__
    bool handle_page_fault(uintptr_t address, uint64_t error_code);
    bool allocate_cr3();
#endif
    bool copy_from(void* ks_addr, const void* us_addr, size_t size);
    bool copy_to(void* us_addr, const void* ks_addr, size_t size);
    void terminate();
    void suspend();
    void resume();
    nt_status add_handle(object& obj, uint32_t access_mask, bool allow_shared,
                        handle_t& h);
    NTSTATUS allocate_vm(void** base_address, uintptr_t zero_bits, size_t* region_size,
                         uint32_t allocation_type, enum mapping_type protect);
    NTSTATUS free_vm(void** base_address, size_t* region_size, uint32_t free_type);
    object_ref deref_handle(handle_t h);
    object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask);
    NTSTATUS close_handle(handle_t h);
    NTSTATUS map_view_of_section(section& sect, void** base_address,
                                 uintptr_t zero_bits, size_t commit_size,
                                 optional<int64_t> section_offset, size_t& view_size,
                                 enum SECTION_INHERIT inherit_disposition,
                                 uint32_t allocation_type, enum mapping_type protect,
                                 bool cow);
    NTSTATUS unmap_view_of_section(void* base_address);
    NTSTATUS create_thread(optional<reference_wrapper<handle_t>> thread_handle,
                           ACCESS_MASK desired_access, const OBJECT_ATTRIBUTES* object_attributes,
                           CLIENT_ID& client_id, const CONTEXT& thread_context, const INITIAL_TEB& initial_teb,
                           bool create_suspended);
    NTSTATUS query_info(enum PROCESSINFOCLASS process_information_class, span<uint8_t> buf,
                        uint32_t* return_length);
    NTSTATUS read_vm(void* base_address, void* buffer, uint32_t buffer_size,
                     uint32_t* number_of_bytes_read);
    NTSTATUS write_vm(void* base_address, const void* buffer, uint32_t buffer_size,
                      uint32_t* number_of_bytes_written);
    nt_status flush_vm(void** base_address, size_t* region_size);
    object_ref get_stderr();
    nt_status get_next_thread(thread* threadptr, ACCESS_MASK desired_access,
                              uint32_t handle_attributes, uint32_t flags,
                              HANDLE* new_thread_handle);

    optional<reference_wrapper<section>> img;
    spinlock mappings_lock;
    list_entry mappings = { &mappings, &mappings };
#ifdef __aarch64__
    uint64_t ttbr0 = 0;
#elifdef __x86_64__
    uint64_t cr3 = 0;
#endif
    spinlock threads_lock;
    list_entry threads = { &threads, &threads };
    enum nt_status exit_status = nt_status::PENDING;
    atomic<unsigned int> suspend_count;
    spinlock handle_lock;
    handle_t last_handle = 0;
    rbtree_node<handle_t>* handles = nullptr;
    PEB* peb = nullptr;
    RTL_USER_PROCESS_PARAMETERS* proc_params = nullptr;
    uintptr_t ntdll_addr, img_addr;
    object_ref vm_sect;
    bool initialized = false;
    rangelock rl;
    atomic<uint64_t> running_cpus = 0;
    rbtree_node<uintptr_t> tree;
    bool added_to_tree = false;
    bool dying = false;
};

static_assert(is_sync_object<process>);

#ifdef __aarch64__
static_assert(offsetof(process, ttbr0) == PROCESS_TTBR0);
#elifdef __x86_64__
static_assert(offsetof(process, cr3) == PROCESS_CR3);
#endif

export object_ref create_process(string_view fn, string_view curdir,
                                 u16string_view cmdline, object_ref& stdinobj,
                                 object_ref& stdoutobj, object_ref& stderrobj);
export void load_ntdll();
export void create_time_page();
