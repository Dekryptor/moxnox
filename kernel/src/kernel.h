#pragma once

#include <cstdint>
#include <string_view>
#include <cstring>
#include <atomic>
#include <functional>
#include <optional>
#include <span>
#include "nt.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

struct loader_block;
class thread;
class object;
struct cpu_block;
struct list_entry;
class unique_handle;
class event;
struct task_state_segment;
struct pci_device;
class semaphore;
class volume;
class filesystem;
class pipe;
struct HARDWARE_PTE;

static const uint32_t PIT_HZ = 1193182;

static const unsigned int KERNEL_STACK_PAGES = 8; // 32KB
static const unsigned int KERNEL_GUARD_PAGES = 1;

static const uint8_t halt_interrupt = 0xfe;
static const uint8_t spurious_interrupt = 0xff;
static const uint8_t yield_interrupt = 0x30;

static const uint64_t kernel_handle_mask = 0x8000000000000000;

static const uint16_t user_CS = 0x3b;
static const uint16_t user_DS = 0x33;

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

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

static const guid null_guid = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

struct handle_obj {
    object* obj;
    uint32_t access_mask;
};

struct page_info {
    uint64_t pa;
    enum mapping_type protect;
};

// mem.cpp
bool init_memory(const loader_block& lb);
bool map_page(uint64_t pa, uint64_t va, enum mapping_type type);
uint64_t allocate_page();
void unmap_and_free_page(uint64_t va);
void free_page(uint64_t pa);
void unmap_page(uint64_t va);
void protect_page(uint64_t va, enum mapping_type type);
void protect_page2(HARDWARE_PTE& pt_entry, enum mapping_type type, bool ks);
bool init_vablock();
uint64_t allocate_va(uint32_t pages);
void free_va(uint64_t va, uint32_t pages);
uint64_t va_to_pa(uint64_t va);
void free_lower_half_mappings();
void invalidate_tlb();

// console.cpp
bool init_console(void* pa, uint32_t width, uint32_t height, uint32_t pixels_per_scan_line);
bool init_console_pipe();
void safe_print(std::string_view s);
extern decltype(&safe_print) print;
void print_hex(uint64_t num, unsigned int padding = 0);
bool init_fb_console();
void bsod(std::string_view msg);

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
extern "C" void halt_isr();
extern "C" void divide_by_zero_isr();
extern "C" void yield_isr();

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
unique_handle add_kernel_thread(void (*func)());
unique_handle add_kernel_thread(void (*func)(void*), void* param);
unique_handle add_kernel_thread(void (*func)(void*, void*), void* param1, void* param2);
extern "C" void scheduler();
std::span<char> sp_context(std::span<char>dest, const CONTEXT& ctx);
void wait_for_object(handle_t h);
void wait_for_object(object& obj);
bool init_cpu_block(bool handle_timers, unsigned int cpu_no);

// handle.cpp
handle_t add_kernel_handle(object& obj, uint32_t access_mask);
void close_handle(handle_t h);
std::optional<std::reference_wrapper<object>> deref_handle(handle_t h);
bool name_object(handle_t h, std::string_view name, bool persistent = false);
bool name_object(object& obj, std::string_view name, bool persistent = false);
unique_handle open_object(std::string_view name);
size_t read(handle_t h, std::span<uint8_t> s, std::optional<size_t> offset = std::nullopt);
void write(handle_t h, std::span<const uint8_t> s);
void write(handle_t h, std::string_view s);
void handle_closed(object& obj);

// smp.cpp
extern uint8_t* apic_ids;
extern unsigned int num_cpus;
bool init_smp();
uint8_t current_cpu_id();

// pe.cpp
void protect_kernel_pages();
enum mapping_type characteristics_to_mapping_type(uint32_t c);

// keyboard.cpp
void init_keyboard();

// event.cpp
unique_handle create_event(bool autoreset);
void set_event(handle_t h);
void set_event(event& e);
void reset_event(handle_t h);

// pipe.cpp
unique_handle create_pipe(unsigned int buffer_pages);
unique_handle create_duplex_pipe(pipe& recv_pipe, pipe& send_pipe);

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
bool mount_root();
unique_handle open_inode(std::string_view fn);

// process.cpp
extern uintptr_t mn_start_thread, mn_signal;
unique_handle create_process(std::string_view fn, std::string_view curdir,
                             std::optional<std::reference_wrapper<object>> stdinobj,
                             std::optional<std::reference_wrapper<object>> stdoutobj,
                             std::optional<std::reference_wrapper<object>> stderrobj);
void load_ntdll();

// section.cpp
unique_handle create_section(handle_t h, bool image, size_t size);

// string.cpp
std::span<char> sp_strcpy(std::span<char> dest, std::string_view src);
std::span<char> sp_itoa_hex(std::span<char> dest, uint64_t num, unsigned int padding = 0);
size_t utf16_to_utf8_len(std::u16string_view u16sv);
void utf16_to_utf8(std::span<char> buf, std::u16string_view u16sv);
size_t utf8_to_utf16_len(std::string_view sv);
void utf8_to_utf16(std::span<char16_t> buf, std::string_view sv);

// syscalls.S
extern "C" void init_syscalls();

// serial.cpp
extern unique_handle com1_pipe_handle;
bool init_serial();

// serial.S
extern "C" bool init_serial_port(uint16_t port);
extern "C" void serial_isr();

// mem.S
extern "C" void memcpy_aligned(void* dest, const void* src, size_t n);
extern "C" void init_pat();

// thread.cpp
thread* get_current_thread();
