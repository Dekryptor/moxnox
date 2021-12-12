#pragma once

#include <stdint.h>
#include <string_view>

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
bool init_interrupts();

// ints.s
extern "C" void dummy_isr();
extern "C" void page_fault_isr();
extern "C" void double_fault_isr();
extern "C" void general_protection_fault_isr();
extern "C" void keyboard_isr();
