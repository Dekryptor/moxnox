#pragma once

#include <cstdint>
#include <string_view>
#include <cstring>
#include <atomic>
#include <functional>
#include <optional>
#include <span>
#include <source_location>
#include <charconv>
#include <loaderblock.h>
#include "nt.h"
#include "list.h"

struct loader_block;
class thread;
class object;
struct cpu_block;
class event;
struct task_state_segment;
struct pci_device;
class semaphore;
class volume;
class filesystem;
class pipe;
struct HARDWARE_PTE;

#define offsetofend(TYPE, MEMBER) (offsetof(TYPE, MEMBER) + sizeof(((TYPE *)0)->MEMBER))

static const uint32_t PIT_HZ = 1193182;

static const unsigned int KERNEL_STACK_PAGES = 8; // 32KB
static const unsigned int KERNEL_GUARD_PAGES = 1;

static const uint8_t yield_interrupt = 0xfc;
static const uint8_t halt_interrupt = 0xfd;
static const uint8_t invalidate_tlb_interrupt = 0x21;
static const uint8_t spurious_interrupt = 0xff;

static const uint16_t user_CS = 0x3b;
static const uint16_t user_DS = 0x33;

using handle_t = uint64_t;

enum class mapping_type {
    no_access,
    read_write,
    read_only,
    read_execute,
    read_write_execute,
    no_cache,
    write_combine,
    reserved
};

struct guid {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];

    bool operator==(const guid& rhs) const noexcept {
        return !memcmp(this, &rhs, sizeof(guid));
    }
};

static_assert(sizeof(guid) == 16);

struct handle_obj {
    object* obj;
    uint32_t access_mask;
};

struct page_info {
    uint64_t pa;
    enum mapping_type protect;
};

struct kernel_image {
    list_entry le;
    void* base_address;
    uint64_t size;
    std::span<const uint8_t> debug_info;
    std::span<const uint8_t> debug_abbrev;
    std::span<const uint8_t> debug_rnglists;
    std::span<const uint8_t> debug_str;
    std::span<const uint8_t> debug_addr;
    std::span<const uint8_t> debug_str_offsets;
    char name[0];
};

// kernel.cpp
extern std::string_view cmdline;
extern list_entry kernel_images;
extern uint64_t rsdp_pa;
extern bool init_smp_finished;

// console.cpp
enum class console_type {
    text,
    framebuffer,
    other
};

bool init_console(const loader_block_framebuffer& fb,
                  const loader_block_text_console& tc);
void safe_print(std::string_view s);
extern void (*_printk)(std::string_view s);
bool init_fb_console();
void copy_edid(loader_block* lb);
extern enum console_type console_type;

// ints.S
extern "C" void page_fault_isr();
extern "C" void double_fault_isr();
extern "C" void general_protection_fault_isr();
extern "C" void invalid_opcode_isr();
extern "C" void spurious_isr();
extern "C" void halt_isr();
extern "C" void divide_by_zero_isr();
extern "C" void yield_isr();
extern "C" void breakpoint_isr();
extern "C" void single_step_isr();
extern volatile uint32_t processors_left_to_halt;

// timer.S
extern "C" void timer_isr();
extern "C" void timer_isr_ap();
extern "C" void yield();
extern "C" void acquire_sync_lock();
extern "C" void release_sync_lock();
extern "C" void pit_delay(uint16_t delay);

// timer.cpp
void init_timer();
void init_lapic_timer(bool ap);
extern volatile int64_t system_time;
extern uint64_t system_time_offset;

class object_ref;

// scheduler.cpp
extern list_entry thread_list, thread_list_dead;
object_ref add_kernel_thread(void (*func)());
object_ref add_kernel_thread(void (*func)(void*), void* param);
object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2);
extern "C" void scheduler();
nt_status wait_for_object(object& obj, bool interruptible);
bool init_cpu_block(unsigned int cpu_no);
void disable_preempt();
void enable_preempt();

// smp.cpp
extern uint8_t* apic_ids;
extern unsigned int num_cpus;
bool init_smp();
uint8_t current_cpu_id();

// pe.cpp
void protect_kernel_pages();
enum mapping_type characteristics_to_mapping_type(uint32_t c);

// event.cpp
object_ref create_event(bool autoreset);
void set_event(event& e);
void reset_event(event& e);

// semaphore.cpp
object_ref create_semaphore(unsigned int maximum_count);
void release_semaphore(semaphore& s);

class inode;

// process.cpp
extern uintptr_t mn_start_thread, mn_signal;
object_ref create_process(std::string_view fn, std::string_view curdir,
                          object_ref& stdinobj, object_ref& stdoutobj,
                          object_ref& stderrobj);
void load_ntdll();

class file;

// section.cpp
nt_status create_section(const std::optional<std::reference_wrapper<file>>& file, bool image,
                         size_t size, object_ref& ref);
uint64_t get_image_size(void* image);

// syscalls.S
extern "C" void init_syscalls();

// mem.S
extern "C" void memcpy_aligned(void* dest, const void* src, size_t n);
extern "C" void init_pat();
extern "C" void invalidate_tlb_isr();

// thread.cpp
thread* get_current_thread();

template<typename T>
concept cmdline_func = std::invocable<T, std::string_view> &&
requires(T t, std::string_view s) { { t(s) } -> std::convertible_to<bool>; };

static void loop_through_cmdline(cmdline_func auto func) {
    std::string_view s = cmdline;

    while (true) {
        if (auto c = s.find_first_not_of(' '); c != std::string_view::npos) {
            s.remove_prefix(c);
        }

        if (s.empty())
            break;

        if (auto c = s.find_first_of(' '); c != std::string_view::npos) {
            if (!func(std::string_view(s.data(), c)))
                break;

            s.remove_prefix(c);
        } else {
            func(s);
            break;
        }
    }
}

struct page_cache_entry;

#ifdef NDEBUG
#define assert(_Expression) ((void)0)
#else
#define assert(_Expression) \
    (void) \
    ((!!(_Expression)) || \
    (_assert(#_Expression,__FILE__,__LINE__),0))
#endif
