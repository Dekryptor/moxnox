module;

#include <span>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <optional>

export module page_cache;

import mem;
import config;
import list;
import rangelock;
import rbtree;
import spinlock;
import nt;
import vec;
import kernel_time_page;

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
concept cacheable = requires(T t, uint64_t page_num, uint64_t num_pages, uint64_t& read_pages) {
    { t.read_into_cache(page_num, num_pages, read_pages) } -> same_as<nt_status>;
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

class page_cache_base {
public:
    page_cache_entry* find_page_cache_entry(uint64_t page_num);
    page_cache_entry* next_page_cache_entry(const page_cache_entry& ent);
    void copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                          span<uint8_t> data, uint64_t offset);
    void copy_into_vec_and_unpin(uint64_t first_page, uint64_t num_pages,
                                 span<const span<uint8_t>> v, uint64_t offset,
                                 optional<uint64_t> size);
    void unpin(uint64_t first_page, uint64_t num_pages);
    bool write_pages_vec_and_unpin(uint64_t first_page, uint64_t num_pages,
                                   span<const span<const uint8_t>>& v,
                                   span<const uint8_t>& top);
    bool write_page_vec_and_unpin(uint64_t first_page, uint64_t inpage_offset,
                                  span<const span<const uint8_t>>& v,
                                  span<const uint8_t>& top, size_t vec_size);
    page_cache_entry* find_page_cache_entry_or_earlier(uint64_t page_num);

    spinlock cache_lock;
    rbtree_node<uint64_t>* root = nullptr;
    rangelock rl;
    spinlock mappings_lock;
    list_entry mappings = { &mappings, &mappings };
};

export
template<typename T>
class page_cache {
public:
    page_cache(T& dev) : dev(dev) { }
    ~page_cache();

    using dev_type = T;

    nt_status pin(uint64_t first_page, uint64_t num_pages,
                  page_cache_entry** first_unread);

    void copy_into_buffer(uint64_t first_page, uint64_t num_pages,
                          span<uint8_t> data, uint64_t offset) {
        base.copy_into_buffer(first_page, num_pages, data, offset);
    }

    void copy_into_vec_and_unpin(uint64_t first_page, uint64_t num_pages,
                                 span<const span<uint8_t>> v, uint64_t offset,
                                 optional<uint64_t> size = nullopt) {
        base.copy_into_vec_and_unpin(first_page, num_pages, v, offset, size);
    }

    bool find_dirty_range(page_cache_entry*& range_start,
                          uint64_t& range_length);

    void unpin(uint64_t first_page, uint64_t num_pages) {
        base.unpin(first_page, num_pages);
    }

    bool write_pages_vec(uint64_t first_page, uint64_t num_pages,
                         span<const span<const uint8_t>>& v,
                         span<const uint8_t>& top);

    page_cache_entry* find_page_cache_entry(uint64_t page_num) {
        return base.find_page_cache_entry(page_num);
    }

    size_t write(span<const uint8_t> data, uint64_t offset);
    size_t write_vec(span<const span<const uint8_t>> v, uint64_t offset);
    nt_status read_into_cache_and_pin(uint64_t first_page, uint64_t num_pages,
                                      uint64_t& read_pages);
    nt_status read(span<const span<uint8_t>> v, uint64_t offset,
                   optional<uint64_t> size = nullopt);

    page_cache_entry* next_page_cache_entry(const page_cache_entry& ent) {
        return base.next_page_cache_entry(ent);
    }

    void mark_dirty();
    nt_status flush(uint64_t page_num, uint64_t num_pages, bool clear_dirty);

    T& dev;
    page_cache_base base;

private:
    nt_status flush_range(uint64_t page_num, uint64_t num_pages);

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
        lock_guard lg(cache.base.cache_lock);

        p = cache.find_page_cache_entry(page_num);
    }

    uint64_t operator*() const override {
        return p->pfn << PAGE_SHIFT;
    }

    page_cache_iterator& operator++() override {
        lock_guard lg(cache->base.cache_lock);

        p = cache->base.next_page_cache_entry(*p);

        return *this;
    }

private:
    page_cache<T>* cache;
    page_cache_entry* p;
};

bool find_unread_range(page_cache_base& cache, page_cache_entry*& range_start,
                       page_cache_entry*& range_end,
                       uint64_t& range_length, page_cache_entry*& ent,
                       uint64_t first_page, uint64_t num_pages);

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
    lock_guard lock(base.cache_lock);

    if (base.root)
        free_cache_entry(base.root);
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
nt_status page_cache<T>::pin(uint64_t first_page, uint64_t num_pages,
                             page_cache_entry** first_unread) {
    lock_guard lg(base.cache_lock);

    page_cache_entry* ent;
    page_cache_entry* p;

    ent = base.find_page_cache_entry(first_page);

    if (!ent) {
        p = create_new_page_cache_entry(first_page);
        if (!p)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (first_unread)
            *first_unread = p;

        insert_into_rbtree(base.root, p->tree);
    } else {
        ent->refcount++;
        p = ent;

        if (first_unread && p->pfn == 0)
            *first_unread = p;
    }

    for (uint64_t page_num = first_page + 1; page_num < first_page + num_pages; page_num++) {
        ent = base.next_page_cache_entry(*p);

        if (!ent || ent->tree.key != page_num) {
            p = create_new_page_cache_entry(page_num);
            if (!p) // FIXME - throw error
                return nt_status::INSUFFICIENT_RESOURCES;

            if (first_unread && !*first_unread)
                *first_unread = p;

            insert_into_rbtree(base.root, p->tree);
        } else {
            ent->refcount++;
            p = ent;

            if (p->pfn == 0 && first_unread && !*first_unread)
                *first_unread = p;
        }
    }

    return nt_status::SUCCESS;
}

export void* get_phys_address(const page_cache_entry& ent);

template<typename T>
void clean_pages(page_cache<T>& cache, page_cache_entry* range_start,
                 uint64_t range_length) {
    lock_guard lg(cache.base.cache_lock);

    auto p = range_start;
    for (uint64_t i = 0; i < range_length; i++) {
        // FIXME - assert that status is page_cache_status::allocated

        p->dirty = 0;
        p = cache.base.next_page_cache_entry(*p);
    }
}

template<typename T>
bool page_cache<T>::write_pages_vec(uint64_t first_page, uint64_t num_pages,
                                    span<const span<const uint8_t>>& v,
                                    span<const uint8_t>& top) {
    // FIXME - add page cache to list of things that need to be flushed

    auto status = pin(first_page, num_pages, nullptr);
    if (!success(status))
        return false; // FIXME - return error

    // copy pages and set as dirty

    if (!base.write_pages_vec_and_unpin(first_page, num_pages, v, top))
        return false;

    mark_dirty();

    return true;
}

template<typename T>
size_t page_cache<T>::write_vec(span<const span<const uint8_t>> v,
                                uint64_t offset) {
    uint64_t read_pages;
    uint64_t written = 0;
    size_t vec_size = get_vec_size(v);

    if (vec_size == 0)
        return 0;

    mark_dirty();

    uint64_t first_page = offset >> PAGE_SHIFT;
    span<const uint8_t> top = v.front();

    if (offset & (PAGE_SIZE - 1)) {
        auto inpage_offset = offset & (PAGE_SIZE - 1);

        auto status = read_into_cache_and_pin(first_page, 1, read_pages);
        if (!success(status) || read_pages == 0)
            return 0; // FIXME - return error

        if (!base.write_page_vec_and_unpin(first_page, inpage_offset, v, top,
                                           vec_size)) {
            return 0;
        }

        written = min(vec_size, PAGE_SIZE - inpage_offset);

        if (written == vec_size)
            return written;

        offset += written;
        first_page = offset >> PAGE_SHIFT;
        vec_size -= written;
    }

    if (vec_size >= PAGE_SIZE) {
        uint64_t num_pages = vec_size >> PAGE_SHIFT;

        if (!write_pages_vec(first_page, num_pages, v, top))
            return written;

        written += num_pages << PAGE_SHIFT;
        vec_size -= num_pages << PAGE_SHIFT;

        if (vec_size == 0)
            return written;

        offset += num_pages << PAGE_SHIFT;
        first_page += num_pages;
    }

    auto status = read_into_cache_and_pin(first_page, 1, read_pages);

    if (!success(status) || read_pages == 0)
        return written;

    if (!base.write_page_vec_and_unpin(first_page, 0, v, top, vec_size))
        return written;

    written += vec_size;

    return written;
}

template<typename T>
size_t page_cache<T>::write(span<const uint8_t> data, uint64_t offset) {
    return write_vec(span(&data, 1), offset);
}

template<typename T>
nt_status read_and_pin(page_cache<T>& cache, uint64_t offset, uint64_t length) {
    if (length == 0)
        return nt_status::SUCCESS;

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + length - 1) >> PAGE_SHIFT) - first_page + 1;

    page_cache_entry* first_unread = nullptr;

    // FIXME - use RB tree rather than linked list
    // FIXME - if create_new_page_cache_entry fails, we should be clearing existing cache

    rangelock_req rlr(cache.base.rl, first_page, num_pages, rangelock_mode::exclusive);

    auto status = cache.pin(first_page, num_pages, &first_unread);
    if (!success(status))
        return status;

    if (first_unread) {
        page_cache_entry* ent = first_unread;

        while (true) {
            page_cache_entry* range_start;
            page_cache_entry* range_end;
            uint64_t range_length;

            if (!find_unread_range(cache.base, range_start, range_end, range_length,
                                   ent, first_page, num_pages))
                break;

            // read data into pages

            {
                uint64_t page_num_start = range_start->tree.key;
                uint64_t pages_left = range_length;

                // FIXME - do this asynchronously (i.e. using all the free slots, and only waiting at end or when none left)
                // FIXME - better if we could allocate contiguous physical pages? (folios?)

                while (pages_left > 0) {
                    uint64_t read_pages;

                    auto status = cache.dev.read_into_cache(page_num_start, pages_left,
                                                            read_pages);

                    if (!success(status))
                        return status;

                    page_num_start += read_pages;
                    pages_left -= read_pages;
                }
            }

            clean_pages(cache, range_start, range_length);

            if (range_end->tree.key == first_page + num_pages - 1)
                break;
        }
    }

    return nt_status::SUCCESS;
}

template<typename T>
nt_status page_cache<T>::read_into_cache_and_pin(uint64_t first_page, uint64_t num_pages,
                                                 uint64_t& read_pages) {
    page_cache_entry* first_unread = nullptr;

    rangelock_req rlr(base.rl, first_page, num_pages, rangelock_mode::exclusive);

    auto status = pin(first_page, num_pages, &first_unread);
    if (!success(status))
        return status;

    if (first_unread) {
        page_cache_entry* ent = first_unread;

        while (true) {
            page_cache_entry* range_start;
            page_cache_entry* range_end;
            uint64_t range_length;

            if (!find_unread_range(base, range_start, range_end, range_length,
                                   ent, first_page, num_pages))
                break;

            // read data into pages

            {
                uint64_t page_num_start = range_start->tree.key;
                uint64_t pages_left = range_length;

                // FIXME - do this asynchronously (i.e. using all the free slots, and only waiting at end or when none left)
                // FIXME - better if we could allocate contiguous physical pages? (folios?)

                while (pages_left > 0) {
                    uint64_t cache_read_pages;

                    auto status = dev.read_into_cache(page_num_start, pages_left,
                                                      cache_read_pages);

                    if (!success(status) || cache_read_pages == 0) {
                        clean_pages(*this, range_start, range_length - pages_left);
                        unpin(page_num_start,
                              first_page + num_pages - page_num_start);
                        read_pages = page_num_start - first_page;

                        if (read_pages > 0)
                            return nt_status::SUCCESS;
                        else
                            return status;
                    }

                    page_num_start += cache_read_pages;
                    pages_left -= cache_read_pages;
                }
            }

            clean_pages(*this, range_start, range_length);

            if (range_end->tree.key == first_page + num_pages - 1)
                break;
        }
    }

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

template<typename T>
nt_status page_cache<T>::read(span<const span<uint8_t>> v, uint64_t offset,
                              optional<uint64_t> size) {
    size_t vec_size = get_vec_size(v);

    if (size.has_value())
        vec_size = min(vec_size, *size);

    if (vec_size == 0)
        return nt_status::SUCCESS;

    auto status = read_and_pin(*this, offset, vec_size);
    if (!success(status))
        return status;

    uint64_t first_page = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + vec_size - 1) >> PAGE_SHIFT) - first_page + 1;

    copy_into_vec_and_unpin(first_page, num_pages, v, offset, size);

    // FIXME - LRU

    return nt_status::SUCCESS;
}

template<typename T>
bool page_cache<T>::find_dirty_range(page_cache_entry*& range_start,
                                     uint64_t& range_length) {
    lock_guard lg(base.cache_lock);

    // find first contiguous dirty range

    if (!base.root)
        return false;

    // get first entry
    auto ent = &rbtree_containing_record<page_cache_entry>(get_tree_minimum(base.root));

    while (!ent->dirty) {
        ent = base.next_page_cache_entry(*ent);

        if (!ent)
            return false;
    }

    range_start = ent;

    range_length = 1;

    while (true) {
        auto ent2 = base.next_page_cache_entry(*ent);

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

            auto now = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

            dirty = true;
            dev.mtime = dev.ctime = now;

            dev.acquire();
            insert_tail_list(dev.fs.writeback_list, dev.writeback_le);
        }
    }
}

nt_status flush_mappings(list_entry& mappings, spinlock& mappings_lock, uint64_t page_num, uint64_t num_pages);

template<typename T>
nt_status page_cache<T>::flush_range(uint64_t page_num, uint64_t num_pages) {
    nt_status status;

    status = flush_mappings(base.mappings, base.mappings_lock, page_num, num_pages);
    if (!success(status))
        return status;

    status = dev.flush_cache(page_num, num_pages);
    if (!success(status))
        return status;

    // clear dirty bits on page cache entries

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(base.cache_lock);

            if (i == 0)
                p = base.find_page_cache_entry(page_num);
            else
                p = base.next_page_cache_entry(*p);
        }

        p->dirty = 0;
    }

    return nt_status::SUCCESS;
}

template<typename T>
nt_status page_cache<T>::flush(uint64_t page_num, uint64_t num_pages,
                               bool clear_dirty) {
    static const uint64_t INVALID = 0xffffffffffffffff;

    rangelock_req rlr(base.rl, page_num, num_pages, rangelock_mode::exclusive);

    page_cache_entry* p = nullptr;
    uint64_t range_start = INVALID, range_length = 0;

    while (true) {
        {
            lock_guard lg(base.cache_lock);

            if (!p) {
                p = base.find_page_cache_entry_or_earlier(page_num);
                if (!p || p->tree.key >= page_num + num_pages)
                    return nt_status::SUCCESS;

                while (p->tree.key < page_num) {
                    p = base.next_page_cache_entry(*p);
                    if (!p || p->tree.key >= page_num + num_pages)
                        return nt_status::SUCCESS;
                }
            } else {
                p = base.next_page_cache_entry(*p);
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
            lock_guard lg(base.cache_lock);
            p->refcount--;
        }
    }

    if (range_start != INVALID)
        flush_range(range_start, range_length);

    if (clear_dirty)
        dirty = false;

    return nt_status::SUCCESS;
}
