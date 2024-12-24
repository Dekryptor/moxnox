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

template<typename T>
concept cacheable = requires(T t, uint64_t page_num, uint64_t num_pages) {
    { t.read_into_cache(page_num, num_pages) } -> std::convertible_to<size_t>;
};

template<typename T>
class page_cache {
public:
    page_cache(T& dev) : dev(dev) { }

    size_t read(std::span<uint8_t> data, uint64_t offset);

    spinlock cache_lock;
    list_entry cache = { &cache, &cache };

private:
    T& dev;
};

page_cache_entry* find_page_cache_entry(list_entry& page_cache, uint64_t page_num);
page_cache_entry* create_new_page_cache_entry(uint64_t page_num);

template<typename T>
size_t page_cache<T>::read(std::span<uint8_t> data, uint64_t offset) {
    if (data.empty())
        return 0;

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + data.size() - 1) >> PAGE_SHIFT) - first_page + 1;

    page_cache_entry* first_unread = nullptr;

    // FIXME - use RB tree rather than linked list
    // FIXME - if create_new_page_cache_entry fails, we should be clearing existing cache

    // FIXME - lock range

    {
        std::lock_guard lg(cache_lock);

        page_cache_entry* ent;
        page_cache_entry* p;

        ent = find_page_cache_entry(cache, first_page);

        if (!ent || ent->page_num != first_page) {
            p = create_new_page_cache_entry(first_page);
            if (!p) // FIXME - throw error
                return 0;

            if (!ent) // first entry
                insert_head_list(cache, p->le);
            else
                insert_head_list(ent->le, p->le);

            if (!first_unread)
                first_unread = p;

        } else {
            ent->refcount++;
            p = ent;

            if (p->status != page_cache_status::clean) {
                if (!first_unread)
                    first_unread = p;
            }
        }

        for (uint64_t page_num = first_page + 1; page_num < first_page + num_pages; page_num++) {
            if (p->le.next == &cache)
                ent = nullptr;
            else
                ent = &containing_record<page_cache_entry>(p->le.next);

            if (!ent || ent->page_num != page_num) {
                p = create_new_page_cache_entry(page_num);
                if (!p) // FIXME - throw error
                    return 0;

                if (!ent) // first entry
                    insert_tail_list(cache, p->le);
                else
                    insert_head_list(*ent->le.prev, p->le);

                if (!first_unread)
                    first_unread = p;
            } else {
                ent->refcount++;
                p = ent;

                if (p->status != page_cache_status::clean) {
                    if (!first_unread)
                        first_unread = p;
                }
            }
        }
    }

    if (first_unread) {
        page_cache_entry* ent = first_unread;

        while (true) {
            page_cache_entry* range_start;
            page_cache_entry* range_end;
            uint64_t range_length;

            {
                std::lock_guard lg(cache_lock);

                // find first contiguous unread range

                while (ent->status != page_cache_status::allocated) {
                    if (ent->le.next == &cache) {
                        ent = nullptr;
                        break;
                    }

                    ent = &containing_record<page_cache_entry>(ent->le.next);
                }

                if (!ent)
                    break;

                range_start = ent;

                uint64_t range_left = first_page + num_pages - range_start->page_num - 1;

                range_length = 1;

                if (range_left != 0) {
                    for (uint64_t i = 0; i < range_left; i++) {
                        ent = &containing_record<page_cache_entry>(ent->le.next);

                        if (ent->status != page_cache_status::allocated) {
                            ent = &containing_record<page_cache_entry>(ent->le.prev);
                            break;
                        }

                        range_length++;

                        if (ent->le.next == &cache)
                            break;
                    }
                }

                range_end = ent;
            }

            // read data into pages

            {
                uint64_t page_num_start = range_start->page_num;
                uint64_t pages_left = range_length;

                // FIXME - do this asynchronously (i.e. using all the free slots, and only waiting at end or when none left)
                // FIXME - better if we could allocate contiguous physical pages? (folios?)

                while (pages_left > 0) {
                    {
                        char s[100];
                        auto sp = std::span(s, sizeof(s));

                        sp = sp_strcpy(sp, "page_cache: do read from ");
                        sp = sp_itoa_hex(sp, page_num_start);
                        sp = sp_strcpy(sp, ", length ");
                        sp = sp_itoa_hex(sp, pages_left);
                        sp = sp_strcpy(sp, "\n");
                        printk(std::string_view(s, sp.data() - s));
                    }

                    auto ret = dev.read_into_cache(page_num_start, pages_left);

                    if (ret == 0) {
                        printk("read_into_cache failed\n");
                        return 0;
                    }

                    page_num_start += ret;
                    pages_left -= ret;
                }
            }

            // change status to clean

            {
                std::lock_guard lg(cache_lock);

                auto p = range_start;
                for (uint64_t i = 0; i < range_length; i++) {
                    // FIXME - assert that status is page_cache_status::allocated

                    p->status = page_cache_status::clean;
                    p = &containing_record<page_cache_entry>(p->le.next);
                }

            }

            if (range_end->page_num == first_page + num_pages - 1)
                break;
        }
    }

    size_t read_bytes = data.size();

    {
        std::lock_guard lg(cache_lock);

        page_cache_entry* ent;

        // FIXME - assert that page numbers are correct

        for (uint64_t i = 0; i < num_pages; i++) {
            if (i == 0)
                ent = find_page_cache_entry(cache, first_page);
            else
                ent = &containing_record<page_cache_entry>(ent->le.next);

            // copy pages into data

            auto ent_va = phys_address(ent->phys_address);

            if (i == 0 && offset & (PAGE_SIZE - 1)) {
                size_t off = offset & (PAGE_SIZE - 1);

                memcpy(data.data(), (void*)(ent->phys_address + off),
                       std::min(data.size(), (size_t)PAGE_SIZE - off));
                data = data.subspan(std::min(data.size(), (size_t)PAGE_SIZE - off));
            } else {
                memcpy(data.data(), ent_va, std::min(data.size(), (size_t)PAGE_SIZE));
                data = data.subspan(std::min(data.size(), (size_t)PAGE_SIZE));
            }

            ent->refcount--;
        }
    }

    // FIXME - LRU

    return read_bytes;
}
