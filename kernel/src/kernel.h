#pragma once

#include <cstdint>
#include <string_view>
#include <cstring>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

typedef struct _loader_block loader_block;

// mem.cpp
bool init_memory(const loader_block& lb);
bool map_page(uint64_t pa, uint64_t va);
uint64_t allocate_page();

// console.cpp
bool init_console();
void print(std::string_view s);
void print_hex(uint64_t num);

// interrupts.cpp
extern volatile uint8_t* const local_apic;
bool init_interrupts();
void add_irq_handler(uint8_t irq, uint8_t vec, uint32_t apic_id);
void add_idt_entry(uint8_t vector, void* func, bool trap);
void enable_irq(uint8_t irq, bool enable);

// ints.s
extern "C" void dummy_isr();
extern "C" void page_fault_isr();
extern "C" void double_fault_isr();
extern "C" void general_protection_fault_isr();
extern "C" void keyboard_isr();
extern "C" void spurious_isr();

// timer.s
extern "C" void pit_isr();
extern "C" void timer_isr();

// timer.cpp
void init_timer();

// scheduler.cpp
bool add_kernel_thread(void (*func)(void*), void* param);
extern "C" void scheduler();
