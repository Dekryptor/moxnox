#include "kernel.h"
#include "objects.h"
#include "page_cache.h"

using namespace std;

page_cache_entry* page_cache::find_page_cache_entry(uint64_t page_num) {
    // returns page cache entry, or previous one if not found

    page_cache_entry* last_p = nullptr;

    auto le = cache.next;
    while (le != &cache) {
        auto& p = containing_record<page_cache_entry>(le);

        if (p.page_num == page_num)
            return &p;

        if (p.page_num > page_num)
            return last_p;

        last_p = &p;
        le = le->next;
    }

    return last_p;
}

static page_cache_entry* create_new_page_cache_entry(uint64_t page_num) {
    auto p = new page_cache_entry;
    if (!p)
        return nullptr;

    p->page_num = page_num;
    p->refcount = 1;
    p->status = page_cache_status::allocated;
    p->phys_address = allocate_page();

    if (!p->phys_address) {
        printk("allocate_page failed\n");
        delete p;
        return nullptr;
    }

    return p;
}

bool page_cache::pin(uint64_t first_page, uint64_t num_pages,
                     page_cache_entry** first_unread) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;
    page_cache_entry* p;

    ent = find_page_cache_entry(first_page);

    if (!ent || ent->page_num != first_page) {
        p = create_new_page_cache_entry(first_page);
        if (!p) // FIXME - throw error
            return false;

        if (!ent) // first entry
            insert_head_list(cache, p->le);
        else
            insert_head_list(ent->le, p->le);

        if (first_unread)
            *first_unread = p;
    } else {
        ent->refcount++;
        p = ent;

        if (first_unread && p->status != page_cache_status::clean)
            *first_unread = p;
    }

    for (uint64_t page_num = first_page + 1; page_num < first_page + num_pages; page_num++) {
        if (p->le.next == &cache)
            ent = nullptr;
        else
            ent = &containing_record<page_cache_entry>(p->le.next);

        if (!ent || ent->page_num != page_num) {
            p = create_new_page_cache_entry(page_num);
            if (!p) // FIXME - throw error
                return false;

            if (!ent) // first entry
                insert_tail_list(cache, p->le);
            else
                insert_head_list(*ent->le.prev, p->le);

            if (first_unread && !*first_unread)
                *first_unread = p;
        } else {
            ent->refcount++;
            p = ent;

            if (p->status != page_cache_status::clean) {
                if (first_unread && !*first_unread)
                    *first_unread = p;
            }
        }
    }

    return true;
}

void page_cache::copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                                  span<uint8_t> data, uint64_t offset) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i == 0)
            ent = find_page_cache_entry(first_page);
        else
            ent = &containing_record<page_cache_entry>(ent->le.next);

        assert(ent->page_num == first_page + i);

        // copy pages into data

        auto ent_va = phys_address(ent->phys_address);

        if (i == 0 && offset & (PAGE_SIZE - 1)) {
            size_t off = offset & (PAGE_SIZE - 1);

            memcpy(data.data(), (uint8_t*)ent_va + off,
                   min(data.size(), (size_t)PAGE_SIZE - off));
            data = data.subspan(min(data.size(), (size_t)PAGE_SIZE - off));
        } else {
            memcpy(data.data(), ent_va, min(data.size(), (size_t)PAGE_SIZE));
            data = data.subspan(min(data.size(), (size_t)PAGE_SIZE));
        }
    }
}

void page_cache::copy_into_buffer_and_unpin(uint64_t first_page, uint64_t num_pages,
                                            span<uint8_t> data, uint64_t offset) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i == 0)
            ent = find_page_cache_entry(first_page);
        else
            ent = &containing_record<page_cache_entry>(ent->le.next);

        assert(ent->page_num == first_page + i);

        // copy pages into data

        auto ent_va = phys_address(ent->phys_address);

        if (i == 0 && offset & (PAGE_SIZE - 1)) {
            size_t off = offset & (PAGE_SIZE - 1);

            memcpy(data.data(), (uint8_t*)ent_va + off,
                   min(data.size(), (size_t)PAGE_SIZE - off));
            data = data.subspan(min(data.size(), (size_t)PAGE_SIZE - off));
        } else {
            memcpy(data.data(), ent_va, min(data.size(), (size_t)PAGE_SIZE));
            data = data.subspan(min(data.size(), (size_t)PAGE_SIZE));
        }

        ent->refcount--;
    }
}

void page_cache::unpin(uint64_t first_page, uint64_t num_pages) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i == 0)
            ent = find_page_cache_entry(first_page);
        else
            ent = &containing_record<page_cache_entry>(ent->le.next);

        assert(ent->page_num == first_page + i);

        ent->refcount--;
    }
}

void page_cache::clean_pages(page_cache_entry* range_start, uint64_t range_length) {
    lock_guard lg(cache_lock);

    auto p = range_start;
    for (uint64_t i = 0; i < range_length; i++) {
        // FIXME - assert that status is page_cache_status::allocated

        p->status = page_cache_status::clean;
        p = &containing_record<page_cache_entry>(p->le.next);
    }
}

bool page_cache::find_unread_range(page_cache_entry*& range_start,
                                   page_cache_entry*& range_end,
                                   uint64_t& range_length, page_cache_entry*& ent,
                                   uint64_t first_page, uint64_t num_pages) {
    lock_guard lg(cache_lock);

    // find first contiguous unread range

    while (ent->status != page_cache_status::allocated) {
        if (ent->le.next == &cache) {
            ent = nullptr;
            return false;
        }

        ent = &containing_record<page_cache_entry>(ent->le.next);
    }

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

    return true;
}

bool page_cache::write_pages(uint64_t first_page, uint64_t num_pages,
                             span<const uint8_t> data) {
    // FIXME - add page cache to list of things that need to be flushed

    if (!pin(first_page, num_pages, nullptr))
        return false;

    // copy pages and set status to dirty

    {
        page_cache_entry* ent;

        lock_guard lg(cache_lock);

        for (uint64_t i = 0; i < num_pages; i++) {
            if (i == 0)
                ent = find_page_cache_entry(first_page);
            else
                ent = &containing_record<page_cache_entry>(ent->le.next);

            assert(ent->page_num == first_page + i);

            auto ent_va = phys_address(ent->phys_address);

            memcpy(ent_va, data.data() + (i << PAGE_SHIFT), PAGE_SIZE);

            ent->status = page_cache_status::dirty;
            ent->refcount--; // unpin
        }
    }

    return true;
}

size_t page_cache::write(std::span<const uint8_t> data, uint64_t offset) {
    if (data.empty())
        return 0;

    // FIXME - handle offset not being aligned to page (need to read)
    // FIXME - handle offset + data.size() not being aligned to page (need to read)

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + data.size() - 1) >> PAGE_SHIFT) - first_page + 1;

    if (!write_pages(first_page, num_pages, data))
        return 0;

    return data.size();
}

bool page_cache::read_and_pin(uint64_t offset, uint64_t length) {
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

uint64_t page_cache::read_into_cache_and_pin(uint64_t page_num, uint64_t num_pages) {
    if (!pin(page_num, num_pages, nullptr))
        return 0;

    auto read_pages = dev.read_into_cache(page_num, num_pages);

    if (read_pages < num_pages)
        unpin(page_num + read_pages, num_pages - read_pages);

    return read_pages;
}

size_t page_cache::read(std::span<uint8_t> data, uint64_t offset) {
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

bool page_cache::find_dirty_range(page_cache_entry*& range_start,
                                  page_cache_entry*& range_end,
                                  uint64_t& range_length, page_cache_entry*& ent) {
    lock_guard lg(cache_lock);

    // find first contiguous dirty range

    if (is_list_empty(cache))
        return false;

    if (!ent)
        ent = &containing_record<page_cache_entry>(cache.next);

    while (ent->status != page_cache_status::dirty) {
        if (ent->le.next == &cache) {
            ent = nullptr;
            return false;
        }

        ent = &containing_record<page_cache_entry>(ent->le.next);
    }

    range_start = ent;

    range_length = 1;

    while (true) {
        auto ent2 = &containing_record<page_cache_entry>(ent->le.next);

        if (ent2->page_num != range_start->page_num + range_length)
            break;

        if (ent2->status != page_cache_status::dirty)
            break;

        ent = ent2;
        range_length++;

        if (ent->le.next == &cache)
            break;
    }

    range_end = ent;

    return true;
}
