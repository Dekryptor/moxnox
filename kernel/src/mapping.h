#pragma once

#include <nt.h>

class process;
class section;

class mapping {
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

    bool map(uint32_t mapping_offset, uint64_t va);
    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
    bool copy_to(uint32_t offset, const void* ks_addr, uint32_t size);
    nt_status flush(uint64_t offset, uint64_t size);
    void clear_write_bit(uint64_t page_num, uint64_t num_pages);

    list_entry le;
    list_entry cache_le;
    uint64_t address;
    uint32_t size;
    uint64_t offset;
    process& proc;
    section& sect;

private:
    std::atomic<unsigned int> refcount = 1;
};

