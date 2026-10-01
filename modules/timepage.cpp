module;

#include <stdint.h>
#include <array>
#include <atomic>

export module timepage;

import pagesize;
import ipv6;

using namespace std;

// FIXME - change this to 0x7ffe0000 and make format compatible with Windows?
export uintptr_t TIME_PAGE_BASE = 0x7ffd0000;

export struct __attribute__((aligned(PAGE_SIZE))) time_page_struct {
    volatile int64_t system_time;
    uint64_t system_time_offset;
    volatile uint64_t system_time_tsc;
    uint64_t tsc_freq;
    uint64_t tick_rate;
    char os_version[32];
    atomic<uint32_t> dns_servers_seq_lock;
    array<ipv6_address, 16> dns_servers;
};
