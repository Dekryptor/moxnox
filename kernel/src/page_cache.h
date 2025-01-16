#pragma once

#include "objects.h"
#include "mem.h"

enum class page_cache_status {
    allocated,
    clean
};

struct page_cache_entry {
    list_entry le;
    uint64_t page_num;
    std::atomic<uint32_t> refcount;
    enum page_cache_status status;
    uint64_t phys_address;
};

class page_cache_base {
public:
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

    spinlock cache_lock;
    list_entry cache = { &cache, &cache };
};

template<typename T>
class page_cache : public page_cache_base {
public:
    page_cache(T& dev) : dev(dev) { }

    bool read_and_pin(uint64_t offset, uint64_t length);
    size_t read(std::span<uint8_t> data, uint64_t offset);

private:
    T& dev;
};

page_cache_entry* find_page_cache_entry(list_entry& page_cache, uint64_t page_num);
page_cache_entry* create_new_page_cache_entry(uint64_t page_num);

template<typename T>
bool page_cache<T>::read_and_pin(uint64_t offset, uint64_t length) {
    if (length == 0)
        return true;

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + length - 1) >> PAGE_SHIFT) - first_page + 1;

    page_cache_entry* first_unread = nullptr;

    // FIXME - use RB tree rather than linked list
    // FIXME - if create_new_page_cache_entry fails, we should be clearing existing cache

    // FIXME - lock range

    if (!pin(first_page, num_pages, &first_unread))
        return false;

    if (first_unread) {
        page_cache_entry* ent = first_unread;

        while (true) {
            page_cache_entry* range_start;
            page_cache_entry* range_end;
            uint64_t range_length;

            if (!find_unread_range(range_start, range_end, range_length, ent,
                                   first_page, num_pages))
                break;

            // read data into pages

            {
                uint64_t page_num_start = range_start->page_num;
                uint64_t pages_left = range_length;

                // FIXME - do this asynchronously (i.e. using all the free slots, and only waiting at end or when none left)
                // FIXME - better if we could allocate contiguous physical pages? (folios?)

                while (pages_left > 0) {
                    auto ret = dev.read_into_cache(page_num_start, pages_left);

                    if (ret == 0) {
                        printk("read_into_cache failed\n");
                        return 0;
                    }

                    page_num_start += ret;
                    pages_left -= ret;
                }
            }

            clean_pages(range_start, range_length);

            if (range_end->page_num == first_page + num_pages - 1)
                break;
        }
    }

    return true;
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
