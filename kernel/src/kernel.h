#pragma once

#include <string_view>
#include <cstring>
#include <atomic>
#include <functional>
#include <optional>
#include <span>
#include <loaderblock.h>
#include "nt.h"
import list;

class thread;
class object;

#define offsetofend(TYPE, MEMBER) (offsetof(TYPE, MEMBER) + sizeof(((TYPE *)0)->MEMBER))

static const uint32_t PIT_HZ = 1193182;

static const unsigned int KERNEL_STACK_PAGES = 8; // 32KB
static const unsigned int KERNEL_GUARD_PAGES = 1;

static const uint8_t yield_interrupt = 0xfc;
static const uint8_t halt_interrupt = 0x60;
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

class module_init {
public:
    module_init(void (*func)()) {
        func();
    }
};

// kernel.cpp
extern std::string_view cmdline;
extern list_entry kernel_images;
extern uint64_t rsdp_pa;
extern bool init_smp_finished;
extern bool shutting_down;

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

class object_ref;

// process.cpp
extern uintptr_t mn_start_thread, mn_signal;
object_ref create_process(std::string_view fn, std::string_view curdir,
                          std::u16string_view cmdline, object_ref& stdinobj,
                          object_ref& stdoutobj, object_ref& stderrobj);
void load_ntdll();

// thread.cpp
thread* get_current_thread();
object_ref add_kernel_thread(void (*func)());
object_ref add_kernel_thread(void (*func)(void*), void* param);
object_ref add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2);

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

#ifdef NDEBUG
#define assert(_Expression) ((void)0)
#else
#define assert(_Expression) \
    (void) \
    ((!!(_Expression)) || \
    (_assert(#_Expression,__FILE__,__LINE__),0))
#endif
