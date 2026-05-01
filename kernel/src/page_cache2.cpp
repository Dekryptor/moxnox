module;

#include <mutex>
#include <stddef.h>
#include "assert.h"

module page_cache;

import bsod;
import mapping;
import print;

using namespace std;

nt_status flush_mappings(list_entry& mappings, spinlock& mappings_lock, uint64_t page_num, uint64_t num_pages) {
    auto last_page = page_num + num_pages - 1;

    // clear write bit on PTEs of processes that have mapped these pages

    struct mapping_list {
        mapping* m;
        list_entry le;
    };

    list_entry list = { &list, &list };

    {
        lock_guard lg(mappings_lock);

        for (auto le = mappings.next; le != &mappings; le = le->next) {
            auto& m = *(mapping*)((uint8_t*)le - offsetof(mapping, cache_le));

            auto mapping_first_page = m.offset >> PAGE_SHIFT;
            auto mapping_last_page = mapping_first_page + (m.size >> PAGE_SHIFT) - 1;

            if (mapping_last_page < page_num || mapping_first_page >= page_num + num_pages)
                continue;

            auto ml = new mapping_list;
            if (!ml) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            ml->m = &m;
            m.acquire();

            insert_tail_list(list, ml->le);
        }
    }

    while (!is_list_empty(list)) {
        auto& ml = containing_record<mapping_list>(list.next);

        remove_entry_list(ml.le);

        auto& m = *ml.m;

        auto mapping_first_page = m.offset >> PAGE_SHIFT;
        auto mapping_last_page = mapping_first_page + (m.size >> PAGE_SHIFT) - 1;

        if (mapping_first_page >= page_num && mapping_last_page <= last_page) // whole mapping
            m.clear_write_bit(0, mapping_last_page - mapping_first_page + 1);
        else if (mapping_first_page < page_num && mapping_last_page > last_page) // middle of mapping
            m.clear_write_bit(page_num - mapping_first_page, last_page - page_num + 1);
        else if (mapping_last_page >= page_num && mapping_last_page <= last_page) // end of mapping
            m.clear_write_bit(page_num - mapping_first_page, mapping_last_page - page_num + 1);
        else if (mapping_first_page >= page_num && mapping_first_page <= last_page) // beginning of mapping
            m.clear_write_bit(0, last_page - mapping_first_page + 1);

        m.release();

        delete &ml;
    }

    return nt_status::SUCCESS;
}

void* get_phys_address(const page_cache_entry& ent) {
    assert(ent.pfn != 0);

    return phys_address(ent.pfn << PAGE_SHIFT);
}

page_cache_entry* page_cache_base::find_page_cache_entry(uint64_t page_num) {
    auto tree = find_rbtree_node(root, page_num);
    if (!tree)
        return nullptr;

    return &rbtree_containing_record<page_cache_entry>(tree);
}

page_cache_entry* page_cache_base::next_page_cache_entry(const page_cache_entry& ent) {
    auto ret = next_rbtree_node(ent.tree);
    if (!ret)
        return nullptr;

    return &rbtree_containing_record<page_cache_entry>(ret);
}
