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
