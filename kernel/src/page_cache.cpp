#include "kernel.h"
#include "page_cache.h"

using namespace std;

page_cache_entry* find_page_cache_entry(list_entry& page_cache, uint64_t page_num) {
    // returns page cache entry, or previous one if not found

    page_cache_entry* last_p = nullptr;

    auto le = page_cache.next;
    while (le != &page_cache) {
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

page_cache_entry* create_new_page_cache_entry(uint64_t page_num) {
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

bool page_cache_base::init_cache(uint64_t first_page, uint64_t num_pages,
                                 page_cache_entry*& first_unread) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;
    page_cache_entry* p;

    ent = find_page_cache_entry(cache, first_page);

    if (!ent || ent->page_num != first_page) {
        p = create_new_page_cache_entry(first_page);
        if (!p) // FIXME - throw error
            return false;

        if (!ent) // first entry
            insert_head_list(cache, p->le);
        else
            insert_head_list(ent->le, p->le);

        first_unread = p;
    } else {
        ent->refcount++;
        p = ent;

        if (p->status != page_cache_status::clean)
            first_unread = p;
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

    return true;
}

void page_cache_base::copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                                       span<uint8_t> data, uint64_t offset) {
    lock_guard lg(cache_lock);

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
                   min(data.size(), (size_t)PAGE_SIZE - off));
            data = data.subspan(min(data.size(), (size_t)PAGE_SIZE - off));
        } else {
            memcpy(data.data(), ent_va, min(data.size(), (size_t)PAGE_SIZE));
            data = data.subspan(min(data.size(), (size_t)PAGE_SIZE));
        }

        ent->refcount--;
    }
}

void page_cache_base::clean_pages(page_cache_entry* range_start, uint64_t range_length) {
    lock_guard lg(cache_lock);

    auto p = range_start;
    for (uint64_t i = 0; i < range_length; i++) {
        // FIXME - assert that status is page_cache_status::allocated

        p->status = page_cache_status::clean;
        p = &containing_record<page_cache_entry>(p->le.next);
    }
}

bool page_cache_base::find_unread_range(page_cache_entry*& range_start,
                                        page_cache_entry*& range_end,
                                        uint64_t& range_length,
                                        page_cache_entry*& ent,
                                        uint64_t first_page,
                                        uint64_t num_pages) {
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
