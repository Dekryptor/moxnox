#pragma once

#include "objects.h"
#include "mem.h"

enum class page_cache_status {
    allocated,
    clean,
    dirty
};

struct page_cache_entry {
    list_entry le;
    uint64_t page_num;
    std::atomic<uint32_t> refcount;
    enum page_cache_status status;
    uint64_t phys_address;
};

class cacheable;

class page_cache_base {
public:
    page_cache_base(cacheable& dev) : dev(dev) { }

    bool pin(uint64_t first_page, uint64_t num_pages,
             page_cache_entry** first_unread);
    void copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                          std::span<uint8_t> data, uint64_t offset);
    void copy_into_buffer_and_unpin(uint64_t first_page, uint64_t num_pages,
                                    std::span<uint8_t> data, uint64_t offset);
    void clean_pages(page_cache_entry* range_start, uint64_t range_length);
    bool find_unread_range(page_cache_entry*& range_start,
                           page_cache_entry*& range_end, uint64_t& range_length,
                           page_cache_entry*& ent, uint64_t first_page,
                           uint64_t num_pages);
    void unpin(uint64_t first_page, uint64_t num_pages);
    bool write_pages(uint64_t first_page, uint64_t num_pages,
                     std::span<const uint8_t> data);
    page_cache_entry* find_page_cache_entry(uint64_t page_num);
    size_t write(std::span<const uint8_t> data, uint64_t offset);
    bool read_and_pin(uint64_t offset, uint64_t length);

    spinlock cache_lock;
    list_entry cache = { &cache, &cache };

private:
    cacheable& dev;
};

template<typename T>
class page_cache : public page_cache_base {
public:
    page_cache(T& dev) : page_cache_base(dev), dev(dev) { }

    size_t read(std::span<uint8_t> data, uint64_t offset);
    uint64_t read_into_cache_and_pin(uint64_t page_num, uint64_t num_pages);

private:
    cacheable& dev;
};

class cacheable {
public:
    virtual size_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;

    page_cache<cacheable> cache{*this};
};

template<typename T>
uint64_t page_cache<T>::read_into_cache_and_pin(uint64_t page_num, uint64_t num_pages) {
    if (!pin(page_num, num_pages, nullptr))
        return 0;

    auto read_pages = dev.read_into_cache(page_num, num_pages);

    if (read_pages < num_pages)
        unpin(page_num + read_pages, num_pages - read_pages);

    return read_pages;
}

template<typename T>
size_t page_cache<T>::read(std::span<uint8_t> data, uint64_t offset) {
    if (data.empty())
        return 0;

    if (!read_and_pin(offset, data.size()))
        return 0;

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + data.size() - 1) >> PAGE_SHIFT) - first_page + 1;

    copy_into_buffer_and_unpin(first_page, num_pages, data, offset);

    // FIXME - LRU

    return data.size();
}
