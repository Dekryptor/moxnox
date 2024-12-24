#include "kernel.h"
#include "page_cache.h"

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
