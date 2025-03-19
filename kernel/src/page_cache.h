#pragma once

#include "objects.h"

enum class page_cache_status {
    allocated,
    clean,
    dirty
};

struct tree_entry {
    tree_entry* parent;
    tree_entry* left;
    tree_entry* right;
    uint64_t key;
};

struct page_cache_entry {
    tree_entry tree;
    std::atomic<uint32_t> refcount;
    enum page_cache_status status;
    uint64_t phys_address;
};

class cacheable;

class page_cache {
public:
    page_cache(cacheable& dev) : dev(dev) { }

    bool pin(uint64_t first_page, uint64_t num_pages,
             page_cache_entry** first_unread);
    void copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                          std::span<uint8_t> data, uint64_t offset);
    void copy_into_buffer_and_unpin(uint64_t first_page, uint64_t num_pages,
                                    std::span<uint8_t> data, uint64_t offset);
    bool find_dirty_range(page_cache_entry*& range_start,
                          uint64_t& range_length);
    void unpin(uint64_t first_page, uint64_t num_pages);
    bool write_pages(uint64_t first_page, uint64_t num_pages,
                     std::span<const uint8_t> data);
    page_cache_entry* find_page_cache_entry(uint64_t page_num);
    size_t write(std::span<const uint8_t> data, uint64_t offset);
    uint64_t read_into_cache_and_pin(uint64_t first_page, uint64_t num_pages);
    size_t read(std::span<uint8_t> data, uint64_t offset);
    page_cache_entry* next_page_cache_entry(const page_cache_entry& ent);

    spinlock cache_lock;
    cacheable& dev;

private:
    tree_entry* root = nullptr;
};

class cacheable {
public:
    virtual size_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual size_t flush_cache(uint64_t page_num, uint64_t num_pages) = 0;

    page_cache cache{*this};
};

void* get_phys_address(const page_cache_entry& ent);
