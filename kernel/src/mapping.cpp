// SPDX-License-Identifier: GPL-2.0-only
module;

#include <atomic>

export module mapping;

import nt;
import list;
import process;
import section;
import rbtree;

#ifdef __aarch64__
import aarch64;
#elifdef __x86_64__
import x86;
#endif

using namespace std;

export class mapping {
public:
    mapping(process& proc, section& sect);
    ~mapping();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    bool try_acquire();
    bool map(uint32_t mapping_offset, uint64_t va);
    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
    bool copy_to(uint32_t offset, const void* ks_addr, uint32_t size);
    nt_status flush(uint64_t offset, uint64_t size);
    void clear_write_bit(uint64_t page_num, uint64_t num_pages);

    rbtree_node<uint64_t> tree;
    list_entry cache_le;
    uint32_t size = 0;
    uint64_t offset = 0;
    bool in_address_space = false;
    process& proc;
    section& sect;

private:
    atomic<unsigned int> refcount = 1;
};

#ifdef __aarch64__
using pte_type = ARM64_HARDWARE_PTE;
#elifdef __x86_64__
using pte_type = HARDWARE_PTE;
#endif

export void unmap_proc_pages(process& proc, uint64_t va, uint64_t num_pages);
export mapping* find_mapping(process& proc, uint64_t address);
export bool make_pte_writable(uintptr_t address, mapping& m, pte_type& pte,
                              bool& send_ipi);
