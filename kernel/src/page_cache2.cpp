module;

#include <mutex>
#include <span>
#include <algorithm>
#include <optional>
#include <stddef.h>
#include <string.h>
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

    mapping* m2 = nullptr;

    {
        lock_guard lg(mappings_lock);

        for (auto le = mappings.next; le != &mappings; le = le->next) {
            auto& m = *(mapping*)((uint8_t*)le - offsetof(mapping, cache_le));

            auto mapping_first_page = m.offset >> PAGE_SHIFT;
            auto mapping_last_page = mapping_first_page + (m.size >> PAGE_SHIFT) - 1;

            if (mapping_last_page < page_num || mapping_first_page >= page_num + num_pages)
                continue;

            if (!m.try_acquire())
                continue;

            auto ml = new mapping_list;
            if (!ml) {
                print("new failed\n");
                m2 = &m;
                break;
            }

            ml->m = &m;

            insert_tail_list(list, ml->le);
        }
    }

    if (m2) { // new failed
        m2->release();

        while (!is_list_empty(list)) {
            auto& ml = containing_record<mapping_list>(list.next);

            remove_entry_list(ml.le);
            ml.m->release();

            delete &ml;
        }

        return nt_status::INSUFFICIENT_RESOURCES;
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

void page_cache_base::copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                                       span<uint8_t> data, uint64_t offset) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i == 0)
            ent = find_page_cache_entry(first_page);
        else
            ent = next_page_cache_entry(*ent);

        assert(ent->tree.key == first_page + i);

        // copy pages into data

        auto ent_va = get_phys_address(*ent);

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

void page_cache_base::copy_into_vec_and_unpin(uint64_t first_page,
                                              uint64_t num_pages,
                                              span<const span<uint8_t>> v,
                                              uint64_t offset,
                                              optional<uint64_t> size) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;
    size_t vec_size = get_vec_size(v);
    span<uint8_t> top = v.empty() ? span<uint8_t>() : v.front();

    if (size.has_value())
        vec_size = min(vec_size, *size);

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i == 0)
            ent = find_page_cache_entry(first_page);
        else
            ent = next_page_cache_entry(*ent);

        assert(ent->tree.key == first_page + i);

        // copy pages into data

        auto ent_va = get_phys_address(*ent);

        if (i == 0 && offset & (PAGE_SIZE - 1)) {
            size_t off = offset & (PAGE_SIZE - 1);

            memcpy_vec(v, top, (uint8_t*)ent_va + off,
                       min(vec_size, (size_t)PAGE_SIZE - off));
            vec_size -= min(vec_size, (size_t)PAGE_SIZE - off);
        } else {
            memcpy_vec(v, top, (uint8_t*)ent_va, min(vec_size, (size_t)PAGE_SIZE));
            vec_size -= min(vec_size, (size_t)PAGE_SIZE);
        }

        ent->refcount--;
    }
}

void page_cache_base::unpin(uint64_t first_page, uint64_t num_pages) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i == 0)
            ent = find_page_cache_entry(first_page);
        else
            ent = next_page_cache_entry(*ent);

        assert(ent->tree.key == first_page + i);

        ent->refcount--;
    }
}

bool page_cache_base::write_pages_vec_and_unpin(uint64_t first_page, uint64_t num_pages,
                                                span<const span<const uint8_t>>& v,
                                                span<const uint8_t>& top) {
    rangelock_req rlr(rl, first_page, num_pages, rangelock_mode::exclusive);

    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache_lock);

            if (i == 0)
                ent = find_page_cache_entry(first_page);
            else
                ent = next_page_cache_entry(*ent);
        }

        assert(ent->tree.key == first_page + i);

        if (ent->pfn)
            free_folio(ent->pfn << PAGE_SHIFT);

        ent->pfn = (allocate_folio(0) >> PAGE_SHIFT) & 0xfffffffffffff;
        if (!ent->pfn) {
            print("allocate_folio failed\n");
            ent->refcount--;
            return false;
        }

        auto ent_va = get_phys_address(*ent);

        memcpy_vec((uint8_t*)ent_va, v, top, PAGE_SIZE);

        ent->dirty = 1;
        ent->refcount--;
    }

    return true;
}

bool page_cache_base::write_page_vec_and_unpin(uint64_t first_page,
                                               uint64_t inpage_offset,
                                               span<const span<const uint8_t>>& v,
                                               span<const uint8_t>& top,
                                               size_t vec_size) {
    rangelock_req rlr(rl, first_page, 1, rangelock_mode::exclusive);

    page_cache_entry* ent;

    {
        lock_guard lg(cache_lock);
        ent = find_page_cache_entry(first_page);
    }

    assert(ent->tree.key == first_page);

    auto pfn = cow_folio(ent->pfn << PAGE_SHIFT) >> PAGE_SHIFT;
    if (!pfn) {
        ent->refcount--; // unpin
        return false;
    }

    ent->pfn = pfn & 0xfffffffffffff;

    auto ent_va = get_phys_address(*ent);

    memcpy_vec((uint8_t*)ent_va + inpage_offset, v, top,
               min(vec_size, PAGE_SIZE - inpage_offset));

    ent->dirty = 1;
    ent->refcount--; // unpin

    return true;
}
