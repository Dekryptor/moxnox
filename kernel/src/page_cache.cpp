#include "kernel.h"
#include "objects.h"
#include "page_cache.h"

import print;
import bsod;
import mem;
import config;
import list;

using namespace std;

static tree_entry* find_tree_entry(tree_entry* root, uint64_t key) {
    tree_entry* ret = root;

    while (ret && ret->key != key) {
        if (key < ret->key)
            ret = ret->left;
        else
            ret = ret->right;
    }

    return ret;
}

static tree_entry* get_tree_minimum(tree_entry* root) {
    tree_entry* ret = root;

    while (ret->left) {
        ret = ret->left;
    }

    return ret;
}

static void left_rotate(tree_entry*& root, tree_entry* x) {
    auto y = x->right;

    x->right = y->left;

    if (y->left)
        y->left->parent = x;

    y->parent = x->parent;

    if (!x->parent)
        root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->left = x;
    x->parent = y;
}

static void right_rotate(tree_entry*& root, tree_entry* x) {
    auto y = x->left;

    x->left = y->right;

    if (y->right)
        y->right->parent = x;

    y->parent = x->parent;

    if (!x->parent)
        root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->right = x;
    x->parent = y;
}

static void insert_fixup(tree_entry*& root, tree_entry* z) {
    while (z->parent && z->parent->colour == tree_colour::red) {
        if (z->parent == z->parent->parent->left) {
            auto y = z->parent->parent->right;

            if (y && y->colour == tree_colour::red) {
                y->colour = tree_colour::black;
                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    left_rotate(root, z);
                }

                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                right_rotate(root, z->parent->parent);
            }
        } else {
            auto y = z->parent->parent->left;

            if (y && y->colour == tree_colour::red) {
                y->colour = tree_colour::black;
                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    right_rotate(root, z);
                }

                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                left_rotate(root, z->parent->parent);
            }
        }
    }

    root->colour = tree_colour::black;
}

static void insert_into_tree(tree_entry*& root, tree_entry& ent) {
    tree_entry* ptr = root;
    tree_entry* new_parent = nullptr;

    while (ptr) {
        new_parent = ptr;

        if (ent.key < ptr->key)
            ptr = ptr->left;
        else
            ptr = ptr->right;
    }

    ent.parent = new_parent;
    ent.left = nullptr;
    ent.right = nullptr;
    ent.colour = tree_colour::red;

    if (!new_parent)
        root = &ent;
    else if (ent.key < new_parent->key)
        new_parent->left = &ent;
    else
        new_parent->right = &ent;

    insert_fixup(root, &ent);
}

static tree_entry* next_tree_entry(const tree_entry& ent) {
    tree_entry* ret;

    if (ent.right) {
        ret = ent.right;

        while (ret->left) {
            ret = ret->left;
        }
    } else {
        const tree_entry* last_ent = &ent;

        ret = last_ent->parent;
        while (ret && ret->right == last_ent) {
            last_ent = ret;
            ret = ret->parent;
        }
    }

    return ret;
}

template<typename T>
T& tree_containing_record(tree_entry* tree) {
    return *(T*)((uint8_t*)tree - offsetof(T, tree));
}

page_cache_entry* page_cache::find_page_cache_entry(uint64_t page_num) {
    auto tree = find_tree_entry(root, page_num);
    if (!tree)
        return nullptr;

    return &tree_containing_record<page_cache_entry>(tree);
}

static page_cache_entry* create_new_page_cache_entry(uint64_t page_num) {
    auto p = new page_cache_entry;
    if (!p)
        return nullptr;

    p->tree.key = page_num;
    p->refcount = 1;
    p->status = page_cache_status::allocated;
    p->phys_address = allocate_page();

    if (!p->phys_address) {
        print("allocate_page failed\n");
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

    if (!ent) {
        p = create_new_page_cache_entry(first_page);
        if (!p) // FIXME - throw error
            return false;

        if (first_unread)
            *first_unread = p;

        insert_into_tree(root, p->tree);
    } else {
        ent->refcount++;
        p = ent;

        if (first_unread && p->status != page_cache_status::clean)
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

            insert_into_tree(root, p->tree);
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

        p->status = page_cache_status::clean;
        p = cache.next_page_cache_entry(*p);
    }
}

static bool find_unread_range(page_cache& cache, page_cache_entry*& range_start,
                              page_cache_entry*& range_end,
                              uint64_t& range_length, page_cache_entry*& ent,
                              uint64_t first_page, uint64_t num_pages) {
    lock_guard lg(cache.cache_lock);

    // find first contiguous unread range

    while (ent->status != page_cache_status::allocated) {
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

            if (!ent || ent->status != page_cache_status::allocated) {
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

    // copy pages and set status to dirty

    {
        page_cache_entry* ent;

        lock_guard lg(cache_lock);

        for (uint64_t i = 0; i < num_pages; i++) {
            if (i == 0)
                ent = find_page_cache_entry(first_page);
            else
                ent = next_page_cache_entry(*ent);

            assert(ent->tree.key == first_page + i);

            auto ent_va = get_phys_address(*ent);

            memcpy(ent_va, data.data() + (i << PAGE_SHIFT), PAGE_SIZE);

            ent->status = page_cache_status::dirty;
            ent->refcount--; // unpin
        }
    }

    return true;
}

size_t page_cache::write(std::span<const uint8_t> data, uint64_t offset) {
    uint64_t written = 0;

    if (data.empty())
        return 0;

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

            ent->status = page_cache_status::dirty;
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

        ent->status = page_cache_status::dirty;
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

    // FIXME - lock range

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

size_t page_cache::read(std::span<uint8_t> data, uint64_t offset) {
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
    auto ent = &tree_containing_record<page_cache_entry>(get_tree_minimum(root));

    while (ent->status != page_cache_status::dirty) {
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

        if (ent2->status != page_cache_status::dirty)
            break;

        ent = ent2;
        range_length++;
    }

    return true;
}

void* get_phys_address(const page_cache_entry& ent) {
    return phys_address(ent.phys_address);
}

page_cache_entry* page_cache::next_page_cache_entry(const page_cache_entry& ent) {
    auto ret = next_tree_entry(ent.tree);
    if (!ret)
        return nullptr;

    return &tree_containing_record<page_cache_entry>(ret);
}
