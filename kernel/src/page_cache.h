#pragma once

import config;

enum class page_cache_status {
    allocated,
    clean,
    dirty
};

struct page_cache_entry {
    rbtree_node<uint64_t> tree;
    std::atomic<uint32_t> refcount;
    enum page_cache_status status;
    struct {
        uint64_t reserved : 12;
        uint64_t pfn : 52;
    };
};

class cacheable;

class page_cache {
public:
    page_cache(cacheable& dev) : dev(dev) { }
    ~page_cache();

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
    rbtree_node<uint64_t>* root = nullptr;
};

class cacheable {
public:
    virtual size_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual size_t flush_cache(uint64_t page_num, uint64_t num_pages) = 0;

    page_cache cache{*this};
};

struct pfn_iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = uint64_t;

    virtual uint64_t operator*() const = 0;
    virtual pfn_iterator& operator++() = 0;

    void operator++(int) { ++*this; }
};

struct page_cache_iterator : public pfn_iterator {
    page_cache_iterator(cacheable& c, uint64_t page_num) : c(&c) {
        std::lock_guard lg(c.cache.cache_lock);

        p = c.cache.find_page_cache_entry(page_num);
    }

    uint64_t operator*() const override {
        return p->pfn << PAGE_SHIFT;
    }

    page_cache_iterator& operator++() override {
        std::lock_guard lg(c->cache.cache_lock);

        p = c->cache.next_page_cache_entry(*p);

        return *this;
    }

private:
    cacheable* c;
    page_cache_entry* p;
};

void* get_phys_address(const page_cache_entry& ent);
