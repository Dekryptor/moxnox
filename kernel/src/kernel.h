#pragma once

#include <cstdint>
#include <string_view>
#include <cstring>
#include <atomic>
#include <functional>
#include <optional>
#include <span>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

typedef struct _loader_block loader_block;
class thread;
class object;
typedef struct _amd64_context amd64_context;
struct cpu_block;

static const uint32_t PIT_HZ = 1193182;

static const unsigned int KERNEL_STACK_PAGES = 4; // 16KB

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

using handle_t = uint64_t;

enum class mapping_type {
    read_write,
    read_only,
    read_execute,
    read_write_execute,
    no_cache
};

// mem.cpp
bool init_memory(const loader_block& lb);
bool map_page(uint64_t pa, uint64_t va, enum mapping_type type);
uint64_t allocate_page();
void unmap_and_free_page(uint64_t va);
void free_page(uint64_t pa);
void unmap_page(uint64_t va);
void protect_page(uint64_t va, enum mapping_type type);

// console.cpp
bool init_console();
void print(std::string_view s);
void print_hex(uint64_t num, unsigned int padding = 0);

// interrupts.cpp
extern volatile uint8_t* const local_apic;
bool init_interrupts();
void add_irq_handler(uint8_t irq, uint8_t vec, uint32_t apic_id);
void add_idt_entry(uint8_t vector, void* func, bool trap);
void enable_irq(uint8_t irq, bool enable);
std::span<const uint8_t> find_madt();

// ints.S
extern "C" void dummy_isr();
extern "C" void page_fault_isr();
extern "C" void double_fault_isr();
extern "C" void general_protection_fault_isr();
extern "C" void invalid_opcode_isr();
extern "C" void keyboard_isr();
extern "C" void spurious_isr();

// timer.S
extern "C" void timer_isr();
extern "C" void yield();
extern "C" void acquire_sync_lock();
extern "C" void release_sync_lock();
extern "C" void pit_delay(uint16_t delay);

// timer.cpp
void init_timer();
void init_lapic_timer(bool set_div);

// scheduler.cpp
bool add_kernel_thread(void (*func)(void*), void* param = nullptr, handle_t* handle = nullptr);
extern "C" void scheduler();
void terminate_thread();
void print_context(const amd64_context& ctx);
void wait_for_object(handle_t h);
bool init_cpu_block();
thread* get_current_thread();

// handle.cpp
handle_t add_handle(object& obj, uint32_t access_mask);
void close_handle(handle_t h);
std::optional<std::reference_wrapper<object>> deref_handle(handle_t h);

// smp.cpp
void init_smp();

// pe.cpp
void protect_kernel_pages();
