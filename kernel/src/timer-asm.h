#pragma once

// timer.S
extern "C" void timer_isr();
extern "C" void timer_isr_ap();
extern "C" void yield();
extern "C" void acquire_sync_lock();
extern "C" void release_sync_lock();
