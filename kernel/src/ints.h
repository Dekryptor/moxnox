#pragma once

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
