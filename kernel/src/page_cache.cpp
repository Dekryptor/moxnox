module;

#include "kernel.h"
#include "spinlock.h"
#include "rangelock.h"
import rbtree;

export module page_cache;

import print;
import bsod;
import mem;
import config;
import list;
import rangelock;

using namespace std;

static spinlock writeback_lock;
static list_entry writeback_list = { &writeback_list, &writeback_list };

export struct page_cache_entry {
    rbtree_node<uint64_t> tree;
    atomic<uint32_t> refcount;
    struct {
        uint64_t dirty : 1;
        uint64_t reserved : 11;
        uint64_t pfn : 52;
    };
};

export class cacheable;

export class page_cache {
public:
    page_cache(cacheable& dev) : dev(dev) { }
    ~page_cache();

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
    size_t write(span<const uint8_t> data, uint64_t offset);
    uint64_t read_into_cache_and_pin(uint64_t first_page, uint64_t num_pages);
    size_t read(span<uint8_t> data, uint64_t offset);
    page_cache_entry* next_page_cache_entry(const page_cache_entry& ent);
    void mark_dirty();

    spinlock cache_lock;
    cacheable& dev;
    rangelock rl;
    list_entry le;

private:
    rbtree_node<uint64_t>* root = nullptr;
    bool dirty = false;
};

export class cacheable {
public:
    virtual size_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual size_t flush_cache(uint64_t page_num, uint64_t num_pages) = 0;

    page_cache cache{*this};
};

export struct pfn_iterator {
    using difference_type = ptrdiff_t;
    using value_type = uint64_t;

    virtual uint64_t operator*() const = 0;
    virtual pfn_iterator& operator++() = 0;

    void operator++(int) { ++*this; }
};

export struct page_cache_iterator : public pfn_iterator {
    page_cache_iterator(cacheable& c, uint64_t page_num) : c(&c) {
        lock_guard lg(c.cache.cache_lock);

        p = c.cache.find_page_cache_entry(page_num);
    }

    uint64_t operator*() const override {
        return p->pfn << PAGE_SHIFT;
    }

    page_cache_iterator& operator++() override {
        lock_guard lg(c->cache.cache_lock);

        p = c->cache.next_page_cache_entry(*p);

        return *this;
    }

private:
    cacheable* c;
    page_cache_entry* p;
};


static void free_cache_entry(rbtree_node<uint64_t>* tree) {
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

page_cache::~page_cache() {
    lock_guard lock(cache_lock);

    if (root)
        free_cache_entry(root);
}

page_cache_entry* page_cache::find_page_cache_entry(uint64_t page_num) {
    auto tree = find_rbtree_node(root, page_num);
    if (!tree)
        return nullptr;

    return &rbtree_containing_record<page_cache_entry>(tree);
}

static page_cache_entry* create_new_page_cache_entry(uint64_t page_num) {
    auto p = new page_cache_entry;
    if (!p)
        return nullptr;

    p->tree.key = page_num;
    p->refcount = 1;
    p->pfn = 0;
    p->dirty = 0;

    return p;
}

bool page_cache::pin(uint64_t first_page, uint64_t num_pages,
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

void page_cache::copy_into_buffer(uint64_t first_page, uint64_t num_pages,
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

void page_cache::copy_into_buffer_and_unpin(uint64_t first_page, uint64_t num_pages,
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

void page_cache::unpin(uint64_t first_page, uint64_t num_pages) {
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

static void clean_pages(page_cache& cache, page_cache_entry* range_start,
                        uint64_t range_length) {
    lock_guard lg(cache.cache_lock);

    auto p = range_start;
    for (uint64_t i = 0; i < range_length; i++) {
        // FIXME - assert that status is page_cache_status::allocated

        p->dirty = 0;
        p = cache.next_page_cache_entry(*p);
    }
}

static bool find_unread_range(page_cache& cache, page_cache_entry*& range_start,
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

bool page_cache::write_pages(uint64_t first_page, uint64_t num_pages,
                             span<const uint8_t> data) {
    // FIXME - add page cache to list of things that need to be flushed

    if (!pin(first_page, num_pages, nullptr))
        return false;

    // copy pages and set as dirty

    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache_lock);

            if (i == 0)
                ent = find_page_cache_entry(first_page);
            else
                ent = next_page_cache_entry(*ent);

            assert(ent->tree.key == first_page + i);

            if (ent->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                ent->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!ent->pfn) {
                    print("allocate_folio failed\n");
                    return false;
                }
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

size_t page_cache::write(span<const uint8_t> data, uint64_t offset) {
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
            lock_guard lg(cache_lock);

            auto ent = find_page_cache_entry(first_page);

            assert(ent->tree.key == first_page);

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
        lock_guard lg(cache_lock);

        auto ent = find_page_cache_entry(first_page);

        assert(ent->tree.key == first_page);

        auto ent_va = get_phys_address(*ent);

        memcpy(ent_va, data.data(), data.size());

        ent->dirty = 1;
        ent->refcount--; // unpin
    }

    written += data.size();

    return written;
}

static bool read_and_pin(page_cache& cache, uint64_t offset, uint64_t length) {
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

uint64_t page_cache::read_into_cache_and_pin(uint64_t first_page, uint64_t num_pages) {
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

size_t page_cache::read(span<uint8_t> data, uint64_t offset) {
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

bool page_cache::find_dirty_range(page_cache_entry*& range_start,
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

page_cache_entry* page_cache::next_page_cache_entry(const page_cache_entry& ent) {
    auto ret = next_rbtree_node(ent.tree);
    if (!ret)
        return nullptr;

    return &rbtree_containing_record<page_cache_entry>(ret);
}

void page_cache::mark_dirty() {
    if (dirty)
        return;

    {
        lock_guard lg(writeback_lock);

        if (dirty)
            return;

        dirty = true;
        insert_tail_list(writeback_list, le);
    }
}
