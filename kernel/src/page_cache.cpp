module;

#include "kernel.h"
#include <span>
#include "spinlock.h"
#include "rangelock.h"
import rbtree;
#include "mapping.h"

export module page_cache;

import print;
import bsod;
import mem;
import config;
import list;
import rangelock;

using namespace std;

export struct page_cache_entry {
    rbtree_node<uint64_t> tree;
    atomic<uint32_t> refcount;
    struct {
        uint64_t dirty : 1;
        uint64_t reserved : 11;
        uint64_t pfn : 52;
    };
};

export
template<typename T>
concept cacheable = requires(T t, uint64_t page_num, uint64_t num_pages) {
    { t.read_into_cache(page_num, num_pages) } -> same_as<size_t>;
};

export
template<typename T>
concept cacheable_flushable = cacheable<T> && requires(T t, uint64_t page_num, uint64_t num_pages) {
    { t.flush_cache(page_num, num_pages) } -> same_as<nt_status>;
};

export
template<typename T>
concept cache_supports_writeback = cacheable_flushable<T> &&
    is_same_v<decltype(T::writeback_le), list_entry>;

export
template<typename T>
class page_cache {
public:
    page_cache(T& dev) : dev(dev) { }
    ~page_cache();

    using dev_type = T;

    bool pin(uint64_t first_page, uint64_t num_pages,
             page_cache_entry** first_unread);
    void copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                          span<uint8_t> data, uint64_t offset);
    void copy_into_buffer_and_unpin(uint64_t first_page, uint64_t num_pages,
                                    span<uint8_t> data, uint64_t offset);
    bool find_dirty_range(page_cache_entry*& range_start,
                          uint64_t& range_length);
    void unpin(uint64_t first_page, uint64_t num_pages);
    bool write_pages(uint64_t first_page, uint64_t num_pages,
                     span<const uint8_t> data);
    page_cache_entry* find_page_cache_entry(uint64_t page_num);
    page_cache_entry* find_page_cache_entry_or_earlier(uint64_t page_num);
    size_t write(span<const uint8_t> data, uint64_t offset);
    uint64_t read_into_cache_and_pin(uint64_t first_page, uint64_t num_pages);
    size_t read(span<uint8_t> data, uint64_t offset);
    page_cache_entry* next_page_cache_entry(const page_cache_entry& ent);
    void mark_dirty();
    nt_status flush(uint64_t page_num, uint64_t num_pages, bool clear_dirty);

    spinlock cache_lock;
    T& dev;
    rangelock rl;
    spinlock mappings_lock;
    list_entry mappings = { &mappings, &mappings };

private:
    nt_status flush_range(uint64_t page_num, uint64_t num_pages);

    rbtree_node<uint64_t>* root = nullptr;
    bool dirty = false;
};

export struct pfn_iterator {
    using difference_type = ptrdiff_t;
    using value_type = uint64_t;

    virtual uint64_t operator*() const = 0;
    virtual pfn_iterator& operator++() = 0;

    void operator++(int) { ++*this; }
};

export
template<typename T>
struct page_cache_iterator : public pfn_iterator {
    page_cache_iterator(page_cache<T>& cache, uint64_t page_num) : cache(&cache) {
        lock_guard lg(cache.cache_lock);

        p = cache.find_page_cache_entry(page_num);
    }

    uint64_t operator*() const override {
        return p->pfn << PAGE_SHIFT;
    }

    page_cache_iterator& operator++() override {
        lock_guard lg(cache->cache_lock);

        p = cache->next_page_cache_entry(*p);

        return *this;
    }

private:
    page_cache<T>* cache;
    page_cache_entry* p;
};

void free_cache_entry(rbtree_node<uint64_t>* tree) {
    auto& r = rbtree_containing_record<page_cache_entry>(tree);

    // FIXME - is recursion going to be a problem here?

    if (tree->left)
        free_cache_entry(tree->left);

    if (tree->right)
        free_cache_entry(tree->right);

    if (r.pfn != 0)
        free_folio(r.pfn << PAGE_SHIFT);

    free(&r);
}

template<typename T>
page_cache<T>::~page_cache() {
    lock_guard lock(cache_lock);

    if (root)
        free_cache_entry(root);
}

template<typename T>
page_cache_entry* page_cache<T>::find_page_cache_entry(uint64_t page_num) {
    auto tree = find_rbtree_node(root, page_num);
    if (!tree)
        return nullptr;

    return &rbtree_containing_record<page_cache_entry>(tree);
}

template<typename T>
page_cache_entry* page_cache<T>::find_page_cache_entry_or_earlier(uint64_t page_num) {
    auto tree = find_rbtree_node_or_less(root, page_num);
    if (!tree)
        return nullptr;

    return &rbtree_containing_record<page_cache_entry>(tree);
}

page_cache_entry* create_new_page_cache_entry(uint64_t page_num) {
    auto p = new page_cache_entry;
    if (!p)
        return nullptr;

    p->tree.key = page_num;
    p->refcount = 1;
    p->pfn = 0;
    p->dirty = 0;

    return p;
}

template<typename T>
bool page_cache<T>::pin(uint64_t first_page, uint64_t num_pages,
                        page_cache_entry** first_unread) {
    lock_guard lg(cache_lock);

    page_cache_entry* ent;
    page_cache_entry* p;

    ent = find_page_cache_entry(first_page);

    if (!ent) {
        p = create_new_page_cache_entry(first_page);
        if (!p) // FIXME - throw error
            return false;

        if (first_unread)
            *first_unread = p;

        insert_into_rbtree(root, p->tree);
    } else {
        ent->refcount++;
        p = ent;

        if (first_unread && p->pfn == 0)
            *first_unread = p;
    }

    for (uint64_t page_num = first_page + 1; page_num < first_page + num_pages; page_num++) {
        ent = next_page_cache_entry(*p);

        if (!ent || ent->tree.key != page_num) {
            p = create_new_page_cache_entry(page_num);
            if (!p) // FIXME - throw error
                return false;

            if (first_unread && !*first_unread)
                *first_unread = p;

            insert_into_rbtree(root, p->tree);
        } else {
            ent->refcount++;
            p = ent;

            if (p->pfn == 0 && first_unread && !*first_unread)
                *first_unread = p;
        }
    }

    return true;
}

export void* get_phys_address(const page_cache_entry& ent) {
    assert(ent.pfn != 0);

    return phys_address(ent.pfn << PAGE_SHIFT);
}

template<typename T>
void page_cache<T>::copy_into_buffer(uint64_t first_page, uint64_t num_pages,
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

template<typename T>
void page_cache<T>::copy_into_buffer_and_unpin(uint64_t first_page, uint64_t num_pages,
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

        ent->refcount--;
    }
}

template<typename T>
void page_cache<T>::unpin(uint64_t first_page, uint64_t num_pages) {
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

template<typename T>
void clean_pages(page_cache<T>& cache, page_cache_entry* range_start,
                 uint64_t range_length) {
    lock_guard lg(cache.cache_lock);

    auto p = range_start;
    for (uint64_t i = 0; i < range_length; i++) {
        // FIXME - assert that status is page_cache_status::allocated

        p->dirty = 0;
        p = cache.next_page_cache_entry(*p);
    }
}

template<typename T>
bool find_unread_range(page_cache<T>& cache, page_cache_entry*& range_start,
                       page_cache_entry*& range_end,
                       uint64_t& range_length, page_cache_entry*& ent,
                       uint64_t first_page, uint64_t num_pages) {
    lock_guard lg(cache.cache_lock);

    // find first contiguous unread range

    while (ent->pfn) {
        ent = cache.next_page_cache_entry(*ent);

        if (!ent)
            return false;
    }

    range_start = ent;

    uint64_t range_left = first_page + num_pages - range_start->tree.key - 1;

    range_length = 1;

    if (range_left != 0) {
        for (uint64_t i = 0; i < range_left; i++) {
            auto orig_ent = ent;

            ent = cache.next_page_cache_entry(*ent);

            if (!ent || ent->pfn) {
                ent = orig_ent;
                break;
            }

            range_length++;
        }
    }

    range_end = ent;

    return true;
}

template<typename T>
bool page_cache<T>::write_pages(uint64_t first_page, uint64_t num_pages,
                                span<const uint8_t> data) {
    // FIXME - add page cache to list of things that need to be flushed

    if (!pin(first_page, num_pages, nullptr))
        return false;

    // copy pages and set as dirty

    {
        rangelock_req rlr(rl, first_page, num_pages);

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

            memcpy(ent_va, data.data() + (i << PAGE_SHIFT), PAGE_SIZE);

            ent->dirty = 1;
            ent->refcount--;
        }
    }

    mark_dirty();

    return true;
}

template<typename T>
size_t page_cache<T>::write(span<const uint8_t> data, uint64_t offset) {
    uint64_t written = 0;

    if (data.empty())
        return 0;

    mark_dirty();

    uint64_t first_page = offset >> PAGE_SHIFT;

    if (offset & (PAGE_SIZE - 1)) {
        auto inpage_offset = offset & (PAGE_SIZE - 1);

        if (read_into_cache_and_pin(first_page, 1) == 0)
            return 0;

        {
            rangelock_req rlr(rl, first_page, 1);

            page_cache_entry* ent;

            {
                lock_guard lg(cache_lock);
                ent = find_page_cache_entry(first_page);
            }

            assert(ent->tree.key == first_page);

            auto pfn = cow_folio(ent->pfn << PAGE_SHIFT) >> PAGE_SHIFT;
            if (!pfn) {
                ent->refcount--; // unpin
                return 0;
            }

            ent->pfn = pfn & 0xfffffffffffff;

            auto ent_va = get_phys_address(*ent);

            memcpy((uint8_t*)ent_va + inpage_offset, data.data(),
                min(data.size(), PAGE_SIZE - inpage_offset));

            ent->dirty = 1;
            ent->refcount--; // unpin
        }

        written = min(data.size(), PAGE_SIZE - inpage_offset);

        if (written == data.size())
            return written;

        offset += written;
        first_page = offset >> PAGE_SHIFT;

        data = data.subspan(written);
    }

    if (data.size() >= PAGE_SIZE) {
        uint64_t num_pages = data.size() >> PAGE_SHIFT;

        if (!write_pages(first_page, num_pages, data))
            return written;

        written += num_pages << PAGE_SHIFT;
        data = data.subspan(num_pages << PAGE_SHIFT);

        if (data.empty())
            return written;

        offset += num_pages << PAGE_SHIFT;
        first_page += num_pages;
    }

    if (read_into_cache_and_pin(first_page, 1) == 0)
        return written;

    {
        rangelock_req rlr(rl, first_page, 1);

        page_cache_entry* ent;

        {
            lock_guard lg(cache_lock);
            ent = find_page_cache_entry(first_page);
        }

        assert(ent->tree.key == first_page);

        auto pfn = cow_folio(ent->pfn << PAGE_SHIFT) >> PAGE_SHIFT;
        if (!pfn) {
            ent->refcount--; // unpin
            return written;
        }

        ent->pfn = pfn & 0xfffffffffffff;

        auto ent_va = get_phys_address(*ent);

        memcpy(ent_va, data.data(), data.size());

        ent->dirty = 1;
        ent->refcount--; // unpin
    }

    written += data.size();

    return written;
}

template<typename T>
bool read_and_pin(page_cache<T>& cache, uint64_t offset, uint64_t length) {
    if (length == 0)
        return true;

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + length - 1) >> PAGE_SHIFT) - first_page + 1;

    page_cache_entry* first_unread = nullptr;

    // FIXME - use RB tree rather than linked list
    // FIXME - if create_new_page_cache_entry fails, we should be clearing existing cache

    // FIXME - lock range

    if (!cache.pin(first_page, num_pages, &first_unread))
        return false;

    if (first_unread) {
        page_cache_entry* ent = first_unread;

        while (true) {
            page_cache_entry* range_start;
            page_cache_entry* range_end;
            uint64_t range_length;

            if (!find_unread_range(cache, range_start, range_end, range_length, ent,
                                   first_page, num_pages))
                break;

            // read data into pages

            {
                uint64_t page_num_start = range_start->tree.key;
                uint64_t pages_left = range_length;

                // FIXME - do this asynchronously (i.e. using all the free slots, and only waiting at end or when none left)
                // FIXME - better if we could allocate contiguous physical pages? (folios?)

                while (pages_left > 0) {
                    auto ret = cache.dev.read_into_cache(page_num_start, pages_left);

                    if (ret == 0) {
                        print("read_into_cache failed\n");
                        return 0;
                    }

                    page_num_start += ret;
                    pages_left -= ret;
                }
            }

            clean_pages(cache, range_start, range_length);

            if (range_end->tree.key == first_page + num_pages - 1)
                break;
        }
    }

    return true;
}

template<typename T>
uint64_t page_cache<T>::read_into_cache_and_pin(uint64_t first_page, uint64_t num_pages) {
    page_cache_entry* first_unread = nullptr;

    rangelock_req rlr(rl, first_page, num_pages);

    if (!pin(first_page, num_pages, &first_unread))
        return 0;

    if (first_unread) {
        page_cache_entry* ent = first_unread;

        while (true) {
            page_cache_entry* range_start;
            page_cache_entry* range_end;
            uint64_t range_length;

            if (!find_unread_range(*this, range_start, range_end, range_length, ent,
                                   first_page, num_pages))
                break;

            // read data into pages

            {
                uint64_t page_num_start = range_start->tree.key;
                uint64_t pages_left = range_length;

                // FIXME - do this asynchronously (i.e. using all the free slots, and only waiting at end or when none left)
                // FIXME - better if we could allocate contiguous physical pages? (folios?)

                while (pages_left > 0) {
                    auto ret = dev.read_into_cache(page_num_start, pages_left);

                    if (ret == 0) {
                        clean_pages(*this, range_start, range_length - pages_left);
                        unpin(page_num_start + ret,
                              first_page + num_pages - page_num_start - ret);
                        return page_num_start + ret - first_page;
                    }

                    page_num_start += ret;
                    pages_left -= ret;
                }
            }

            clean_pages(*this, range_start, range_length);

            if (range_end->tree.key == first_page + num_pages - 1)
                break;
        }
    }

    return num_pages;
}

template<typename T>
size_t page_cache<T>::read(span<uint8_t> data, uint64_t offset) {
    if (data.empty())
        return 0;

    if (!read_and_pin(*this, offset, data.size()))
        return 0;

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + data.size() - 1) >> PAGE_SHIFT) - first_page + 1;

    copy_into_buffer_and_unpin(first_page, num_pages, data, offset);

    // FIXME - LRU

    return data.size();
}

template<typename T>
bool page_cache<T>::find_dirty_range(page_cache_entry*& range_start,
                                     uint64_t& range_length) {
    lock_guard lg(cache_lock);

    // find first contiguous dirty range

    if (!root)
        return false;

    // get first entry
    auto ent = &rbtree_containing_record<page_cache_entry>(get_tree_minimum(root));

    while (!ent->dirty) {
        ent = next_page_cache_entry(*ent);

        if (!ent)
            return false;
    }

    range_start = ent;

    range_length = 1;

    while (true) {
        auto ent2 = next_page_cache_entry(*ent);

        if (!ent2)
            break;

        if (ent2->tree.key != range_start->tree.key + range_length)
            break;

        if (!ent2->dirty)
            break;

        ent = ent2;
        range_length++;
    }

    return true;
}

template<typename T>
page_cache_entry* page_cache<T>::next_page_cache_entry(const page_cache_entry& ent) {
    auto ret = next_rbtree_node(ent.tree);
    if (!ret)
        return nullptr;

    return &rbtree_containing_record<page_cache_entry>(ret);
}

template<typename T>
void page_cache<T>::mark_dirty() {
    if constexpr (!cache_supports_writeback<T>)
        dirty = true;
    else {
        if (dirty)
            return;

        {
            lock_guard lg(dev.fs.writeback_lock);

            if (dirty)
                return;

            dirty = true;

            dev.acquire();
            insert_tail_list(dev.fs.writeback_list, dev.writeback_le);
        }
    }
}

template<typename T>
nt_status page_cache<T>::flush_range(uint64_t page_num, uint64_t num_pages) {
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
            m.clear_write_bit(page_num - mapping_first_page, last_page - mapping_first_page + 1);
        else if (mapping_last_page >= page_num && mapping_last_page <= last_page) // end of mapping
            m.clear_write_bit(page_num - mapping_first_page, mapping_last_page - page_num + 1);
        else if (mapping_first_page >= page_num && mapping_first_page <= last_page) // beginning of mapping
            m.clear_write_bit(0, last_page - mapping_first_page + 1);

        m.release();

        delete &ml;
    }

    auto status = dev.flush_cache(page_num, num_pages);
    if (!success(status))
        return status;

    // clear dirty bits on page cache entries

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache_lock);

            if (i == 0)
                p = find_page_cache_entry(page_num);
            else
                p = next_page_cache_entry(*p);
        }

        p->dirty = 0;
    }

    return nt_status::SUCCESS;
}

template<typename T>
nt_status page_cache<T>::flush(uint64_t page_num, uint64_t num_pages,
                               bool clear_dirty) {
    static const uint64_t INVALID = 0xffffffffffffffff;

    rangelock_req rlr(rl, page_num, num_pages);

    page_cache_entry* p = nullptr;
    uint64_t range_start = INVALID, range_length = 0;

    while (true) {
        {
            lock_guard lg(cache_lock);

            if (!p) {
                p = find_page_cache_entry_or_earlier(page_num);
                if (!p || p->tree.key >= page_num + num_pages)
                    return nt_status::SUCCESS;

                while (p->tree.key < page_num) {
                    p = next_page_cache_entry(*p);
                    if (!p || p->tree.key >= page_num + num_pages)
                        return nt_status::SUCCESS;
                }
            } else {
                p = next_page_cache_entry(*p);
                if (!p || p->tree.key >= page_num + num_pages)
                    break;
            }

            p->refcount++;
        }

        if (range_start != INVALID && p->dirty && p->tree.key == range_start + range_length) {
            range_length++;
            goto next;
        }

        if (range_start != INVALID)
            flush_range(range_start, range_length);

        if (p->dirty) {
            range_start = p->tree.key;
            range_length = 1;
        } else
            range_start = INVALID;

next:
        {
            lock_guard lg(cache_lock);
            p->refcount--;
        }
    }

    if (range_start != INVALID)
        flush_range(range_start, range_length);

    if (clear_dirty)
        dirty = false;

    return nt_status::SUCCESS;
}
