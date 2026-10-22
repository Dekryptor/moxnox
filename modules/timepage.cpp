// SPDX-License-Identifier: MIT
module;

#include <stdint.h>
#include <string.h>
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
    char os_hash[32];
    atomic<uint32_t> dns_servers_seq_lock;
    array<ipv6_address, 16> dns_servers;
    atomic<uint32_t> hostname_seq_lock;
    char hostname[256];
};

static_assert(sizeof(time_page_struct) <= PAGE_SIZE);

#ifndef KERNEL

export decltype(time_page_struct::dns_servers) get_dns_servers() {
    decltype(time_page_struct::dns_servers) ret;

    auto& p = *(time_page_struct*)TIME_PAGE_BASE;

    while (true) {
        auto seq1 = p.dns_servers_seq_lock.load(memory_order_acquire);
        if (seq1 & 1)
            continue;

        memcpy(ret.data(), p.dns_servers.data(), p.dns_servers.size() * sizeof(ipv6_address));

        atomic_thread_fence(memory_order_acquire);

        auto seq2 = p.dns_servers_seq_lock.load(memory_order_relaxed);
        if (seq1 != seq2)
            continue;

        break;
    }

    return ret;
}

export void read_host_name(char* s) {
    auto& p = *(time_page_struct*)TIME_PAGE_BASE;

    while (true) {
        auto seq1 = p.hostname_seq_lock.load(memory_order_acquire);
        if (seq1 & 1)
            continue;

        strcpy(s, p.hostname);

        atomic_thread_fence(memory_order_acquire);

        auto seq2 = p.hostname_seq_lock.load(memory_order_relaxed);
        if (seq1 != seq2)
            continue;

        break;
    }
}

#endif
