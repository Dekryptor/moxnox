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
struct list_entry;
class unique_handle;
class event;
struct task_state_segment;
struct pci_device;
class semaphore;
class volume;
class filesystem;

static const uint32_t PIT_HZ = 1193182;

static const unsigned int KERNEL_STACK_PAGES = 4; // 16KB
static const unsigned int KERNEL_GUARD_PAGES = 1;

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
bool init_vablock();
uint64_t allocate_va(uint32_t pages);
void free_va(uint64_t va, uint32_t pages);
uint64_t va_to_pa(uint64_t va);
void free_lower_half_mappings();
void invalidate_tlb();

// console.cpp
bool init_console();
void safe_print(std::string_view s);
extern decltype(&safe_print) print;
void print_hex(uint64_t num, unsigned int padding = 0);

// interrupts.cpp
extern volatile uint8_t* const local_apic;
bool init_interrupts();
void add_irq_handler(uint8_t irq, uint8_t vec, uint32_t apic_id);
void add_idt_entry(uint8_t vector, void* func, bool trap, uint8_t ist);
void enable_irq(uint8_t irq, bool enable);
std::span<const uint8_t> find_madt();
task_state_segment* init_tss();
void* allocate_stack();

// ints.S
extern "C" void dummy_isr();
extern "C" void page_fault_isr();
extern "C" void double_fault_isr();
extern "C" void general_protection_fault_isr();
extern "C" void invalid_opcode_isr();
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
unique_handle create_timer();
void set_timer(handle_t h, uint64_t timeout, bool periodic);

// scheduler.cpp
extern list_entry thread_list;
unique_handle add_kernel_thread(void (*func)(void*), void* param = nullptr);
extern "C" void scheduler();
void terminate_thread();
void print_context(const amd64_context& ctx);
void wait_for_object(handle_t h);
void wait_for_object(object& obj);
bool init_cpu_block(bool handle_timers, unsigned int cpu_no);
thread* get_current_thread();

// handle.cpp
handle_t add_handle(object& obj, uint32_t access_mask);
void close_handle(handle_t h);
std::optional<std::reference_wrapper<object>> deref_handle(handle_t h);
bool name_object(handle_t h, std::string_view name, bool persistent = false);
bool name_object(object& obj, std::string_view name, bool persistent = false);
unique_handle open_object(std::string_view name);
size_t read(handle_t h, std::span<uint8_t> s, std::optional<size_t> offset = std::nullopt);
void write(handle_t h, std::span<const uint8_t> s);
void write(handle_t h, std::string_view s);

// smp.cpp
void init_smp();
uint8_t current_cpu_id();

// pe.cpp
void protect_kernel_pages();

// keyboard.cpp
void init_keyboard();

// event.cpp
unique_handle create_event(bool autoreset);
void set_event(handle_t h);
void set_event(event& e);

// pipe.cpp
unique_handle create_pipe(unsigned int buffer_pages);

// pci.cpp
void init_pci();

// sata.cpp
void init_sata(const pci_device& dev);

// keyboard.S
extern "C" void keyboard_isr();

// sata.S
extern "C" void sata_isr();

// semaphore.cpp
unique_handle create_semaphore(unsigned int maximum_count);
void release_semaphore(handle_t h);
void release_semaphore(semaphore& s);

// btrfs.cpp
bool btrfs_probe(volume& vol);

// volume.cpp
void add_fs_to_list(filesystem& fs);
