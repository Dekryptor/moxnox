module;

#include <shared_mutex>
#include <variant>
#include "kernel.h"
import objects;
#include "handle.h"
#include "crc32c.h"
#include "tick_rate.h"
#include "spinlock.h"
import rbtree;
#include "thread.h"
import page_cache;
#include "inode.h"
import page_cache;
#include "handle.h"

export module btrfs;

import print;
import format;
import bsod;
import mem;
import config;
import list;
import vfs;
import string;
import timer;
import volume;
import rwsem;

#include "btrfs.h"

using namespace std;

static const uint32_t S_IFREG = 0100000;
static const uint32_t S_IFDIR = 0040000;

static const uint64_t superblock_addrs[] = { 0x10000, 0x4000000, 0x4000000000, 0x4000000000000 };

// FIXME - BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL
// FIXME - BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO
// FIXME - BTRFS_FEATURE_INCOMPAT_COMPRESS_ZSTD
static const uint64_t supported_incompat_flags = BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF |
                                                 BTRFS_FEATURE_INCOMPAT_BIG_METADATA |
                                                 BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF |
                                                 BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA |
                                                 BTRFS_FEATURE_INCOMPAT_NO_HOLES;

// FIXME - BTRFS_FEATURE_COMPAT_RO_VERITY
static const uint64_t supported_compat_ro_flags = BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE |
                                                  BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE_VALID |
                                                  BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE;

static bool btrfs_probe(volume& v);

static filesystem_recognizer mod_filesystem_type = {
    {}, btrfs_probe
};

static void init_btrfs() {
    insert_tail_list(filesystem_types, mod_filesystem_type.le);
}

static module_init module_init_btrfs(init_btrfs);

static nt_status add_new_level(btrfs_filesystem& fs, btrfs_path& p, root& r,
                               uint8_t level);
static nt_status insert_internal_node(btrfs_filesystem& fs, btrfs_path& p,
                                      root& r, uint8_t level, const btrfs_key& k,
                                      uint64_t address);

static bool check_checksum(const btrfs_super_block& sb) {
    auto csum = ~calc_crc32c(0xffffffff, span((const uint8_t*)&sb.fsid, sizeof(btrfs_super_block) - sizeof(sb.csum)));

    return csum == *(uint32_t*)&sb;
}

struct folio_iter : public pfn_iterator {
    folio_iter(uint64_t page_num) : page_num(page_num) { }

    uint64_t operator*() const override {
        return page_num;
    }

    folio_iter& operator++() override {
        page_num++;

        return *this;
    }

private:
    uint64_t page_num;
};

template<>
consteval bool format_spec_valid<btrfs_key>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const btrfs_key& k) {
    sp = do_format_arg(sp, "", "(");
    sp = do_format_arg(sp, ":x", k.objectid);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, ":x", (uint8_t)k.type);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, ":x", k.offset);
    sp = do_format_arg(sp, "", ")");

    return sp;
}

btrfs_filesystem::btrfs_filesystem(volume& vol) : filesystem(&vol) {
    vol.acquire();

    auto sb_pa = allocate_folio(bytes_to_folio_order(sizeof(btrfs_super_block)));
    if (!sb_pa) {
        print("allocate_folio failed\n");
        return;
    }

    sb = (btrfs_super_block*)phys_address(sb_pa);

    auto iter = folio_iter(sb_pa);

    uint64_t read_pages;

    auto status = vol.dio_read(superblock_addrs[0] >> PAGE_SHIFT,
                               bytes_to_pages(sizeof(btrfs_super_block)),
                               iter, read_pages);
    if (!success(status)) {
        print("volume::dio_read returned {}\n", status);
        return;
    }

    if (sb->magic != BTRFS_MAGIC)
        return;

    // FIXME - if checksum fails, try other superblock addresses
    if (!check_checksum(*sb)) {
        print("Incorrect superblock checksum\n");
        return;
    }

    // FIXME - check other superblock addresses, and choose one with highest generation

    if (sb->incompat_flags & ~supported_incompat_flags) {
        print("Unsupported incompat flags\n");
        return;
    }

    if (!(sb->incompat_flags & BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF)) {
        print("Not mounting as BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF not set\n");
        return;
    }

    // FIXME - support free space cache?
    if (!(sb->compat_ro_flags & (BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE | BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE_VALID))) {
        print("free space cache not supported\n");
        return;
    }

    if (!readonly && sb->compat_ro_flags & ~supported_compat_ro_flags) {
        print("Unsupported compat_ro_flags {:x}, mounting readonly\n", sb->compat_ro_flags & ~supported_compat_ro_flags);
        readonly = true;
    }

    sb->incompat_flags |= BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA;

    if (sb->num_devices != 1) {
        print("Multiple devices not supported\n");
        return;
    }

    if (sb->nodesize & (PAGE_SIZE - 1)) {
        print("btrfs node size not multiple of CPU page size\n");
        return;
    }

    if (sb->nodesize & (sb->nodesize - 1)) {
        print("btrfs node size {:x} not a power of two\n", sb->nodesize);
        return;
    }

    node_folio_order = (uint8_t)((sizeof(uint32_t) * 8) - (uint8_t)countl_zero((uint32_t)sb->nodesize) - 1 - PAGE_SHIFT);

    if (node_folio_order > MAX_FOLIO_ORDER) {
        print("btrfs metadata folio order {} larger than maximum {}\n", node_folio_order, MAX_FOLIO_ORDER);
        return;
    }

    // FIXME - subpage on ARM?
    if (sb->sectorsize != PAGE_SIZE) {
        print("btrfs sector size is not CPU page size\n");
        return;
    }

    if (sb->csum_type != 0) {
        print("unsupported csum type {}\n", sb->csum_type);
        return;
    }

    // FIXME - make sure sb->n < sizeof(sb->sys_chunk_array)
    auto chunk_array = span(sb->sys_chunk_array.data(), sb->sys_chunk_array_size);

    while (!chunk_array.empty()) {
        if (chunk_array.size() < sizeof(btrfs_key)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& k = *(btrfs_key*)chunk_array.data();

        if (k.type != key_type::CHUNK_ITEM) {
            print("Unexpected item in chunk array\n");
            return;
        }

        chunk_array = chunk_array.subspan(sizeof(btrfs_key));

        if (chunk_array.size() < offsetof(btrfs_chunk, stripe)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& ci = *(btrfs_chunk*)chunk_array.data();
        size_t chunk_size = offsetof(btrfs_chunk, stripe) + (sizeof(btrfs_stripe) * ci.num_stripes);

        if (chunk_array.size() < chunk_size) {
            print("Chunk array truncated\n");
            return;
        }

        if (ci.type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID1 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6 | BTRFS_BLOCK_GROUP_RAID1C3 | BTRFS_BLOCK_GROUP_RAID1C4)) {
            print("RAID not supported\n");
            return;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            print("malloc failed\n");
            return;
        }

        auto& c = *cptr;
        new (&c) chunk;

        c.offset = k.offset;
        memcpy(&c.ci, chunk_array.data(), chunk_size);
        insert_tail_list(chunk_list, c.le);

        chunk_array = chunk_array.subspan(chunk_size);
    }

    {
        auto rptr = new root;
        if (!rptr) {
            print("new failed\n");
            return;
        }

        auto& r = *rptr;
        r.id = BTRFS_ROOT_TREE_OBJECTID;
        r.ri.bytenr = sb->root;
        r.ri.level = sb->root_level;

        root_root = &r;
        insert_tail_list(root_list, r.le);
    }

    {
        auto rptr = new root;
        if (!rptr) {
            print("new failed\n");
            return;
        }

        auto& r = *rptr;
        r.id = BTRFS_CHUNK_TREE_OBJECTID;
        r.ri.bytenr = sb->chunk_root;
        r.ri.level = sb->chunk_root_level;

        chunk_root = &r;
        insert_tail_list(root_list, r.le);
    }

    if (!load_chunks()) {
        print("load_chunks failed\n");
        return;
    }

    if (!load_roots()) {
        print("load_roots failed\n");
        return;
    }

    // FIXME - default subvol

    root_inode = new btrfs_inode(*this, BTRFS_FS_TREE_OBJECTID, SUBVOL_ROOT_INODE,
                                 btrfs_inode_type::directory);
    if (!root_inode) {
        print("new failed\n");
        return;
    }

    if (!root_inode->load_inode())
        return;

    insert_tail_list(inode_list, root_inode->le);

    sb->generation++;

    // clear log tree (FIXME - replay this)
    sb->log_root = 0;
    sb->log_root_level = 0;

    valid = true;

    // FIXME - set readonly if underlying disk is RO

    if (!readonly) {
        refcount++;

        flush_thread_obj = add_kernel_thread([](void* ctx) {
            auto& fs = *(btrfs_filesystem*)ctx;

            fs.flush_thread();

            if (fs.refcount.fetch_sub(1) == 1)
                delete &fs;
        }, this);
    }
}

static bool btrfs_probe(volume& vol) {
    // FIXME - need quick is_btrfs function before we do allocations?

    auto fs = new btrfs_filesystem(vol);
    if (!fs) {
        print("new failed\n");
        return false;
    }

    if (!fs->valid) {
        delete fs;
        return false;
    }

    add_fs_to_list(*fs);

    vol.filesystem = *fs;

    return true;
}

nt_status btrfs_filesystem::read_metadata(uint64_t addr, span<uint8_t> data) {
    assert(data.size() == sb->nodesize);

    if (!cache.read(data, addr)) {
        print("Short read\n");
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - need locking

    const auto& th = *(btrfs_header*)data.data();
    if (th.flags & HEADER_FLAG_WRITTEN) {
        auto crc32 = ~calc_crc32c(0xffffffff, span((const uint8_t*)&th.fsid,
                                  sb->nodesize - sizeof(th.csum)));

        if (*(uint32_t*)&th != crc32) {
            print("metadata csum error\n");
            // FIXME - evict from cache
            // FIXME - try to read other copies if present
            // FIXME - if success, copy good copies over bad
            // FIXME - increase dev stats counter
            return nt_status::DISK_CORRUPT_ERROR;
        }
    }

    // FIXME - check address, level, and subvol ID
    // FIXME - if duff, read from other version (and replace)

    return nt_status::SUCCESS;
}

bool btrfs_filesystem::load_chunks() {
    list_entry new_chunk_list = { &new_chunk_list, &new_chunk_list };

    btrfs_path p(sb->nodesize);

    auto status = find_item(*chunk_root, {0, key_type::CHUNK_ITEM, 0}, p, false);
    if (!success(status))
        return false;

    bool fail = false;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.type != key_type::CHUNK_ITEM) {
            if (!move_path_forward(p))
                break;

            continue;
        }

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        if (data.size() < offsetof(btrfs_chunk, stripe)) {
            print("Chunk item truncated\n");
            fail = true;
            break;
        }

        auto& ci = *(btrfs_chunk*)data.data();
        size_t chunk_size = offsetof(btrfs_chunk, stripe) + (sizeof(btrfs_stripe) * ci.num_stripes);

        if (data.size() < chunk_size) {
            print("Chunk array truncated\n");
            fail = true;
            break;
        }

        if (ci.type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID1 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6 | BTRFS_BLOCK_GROUP_RAID1C3 | BTRFS_BLOCK_GROUP_RAID1C4)) {
            print("RAID not supported\n");
            fail = true;
            break;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            print("malloc failed\n");
            fail = true;
            break;
        }

        auto& c = *cptr;
        new (&c) chunk;

        c.offset = k.offset;
        memcpy(&c.ci, data.data(), chunk_size);
        insert_tail_list(new_chunk_list, c.le);

        if (!move_path_forward(p))
            break;
    } while (true);

    if (fail) {
        swap_lists(chunk_list, new_chunk_list);
        free_chunk_list();
        swap_lists(chunk_list, new_chunk_list);
        return false;
    }

    free_chunk_list();
    swap_lists(chunk_list, new_chunk_list);

    return true;
}

static chunk* find_chunk(const btrfs_filesystem& fs, uint64_t address) {
    for (auto le = fs.chunk_list.next; le != &fs.chunk_list; le = le->next) {
        auto& c = containing_record<chunk>(le);

        if (address >= c.offset && address < c.offset + c.ci.length)
            return &c;
    }

    return nullptr;
}

uint64_t btrfs_filesystem::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    uint64_t addr = page_num << PAGE_SHIFT;

    // find chunk

    auto pc = find_chunk(*this, addr);

    if (!pc) {
        print("Could not find chunk for logical address {:x}\n", addr);
        return 0;
    }

    auto& c = *pc;

    if (c.ci.type & BTRFS_BLOCK_GROUP_RAID0) {
        print("RAID0 not supported\n");
        return 0;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID10) {
        print("RAID10 not supported\n");
        return 0;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID5) {
        print("RAID10 not supported\n");
        return 0;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID6) {
        print("RAID10 not supported\n");
        return 0;
    }

    // chunk is SINGLE, DUP, RAID1, RAID1C3, or RAID1C4

    // FIXME - choose stripe and device
    // FIXME - handle missing devices

    uint64_t disk_addr = addr - c.offset + c.ci.stripe[0].offset;

    if (addr + (num_pages << PAGE_SHIFT) > c.offset + c.ci.length)
        num_pages = (c.offset + c.ci.length - addr) >> PAGE_SHIFT;

    auto pages_read = vol->cache.read_into_cache_and_pin(disk_addr >> PAGE_SHIFT,
                                                         num_pages);
    if (pages_read == 0)
        return 0;

    // copy from buffer into our own page cache

    // FIXME - can we steal pages?

    page_cache_entry* dest_p;
    page_cache_entry* src_p;

    for (uint64_t i = 0; i < pages_read; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);

            if (dest_p->pfn == 0) {
                auto pa = allocate_folio(0);
                if (!pa) {
                    pages_read = i;
                    break;
                }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            }
        }

        {
            lock_guard lg(vol->cache.cache_lock);

            if (i == 0)
                src_p = vol->cache.find_page_cache_entry(disk_addr >> PAGE_SHIFT);
            else
                src_p = vol->cache.next_page_cache_entry(*src_p);
        }

        auto src_va = get_phys_address(*src_p);
        auto dest_va = get_phys_address(*dest_p);

        memcpy(dest_va, src_va, PAGE_SIZE);
    }

    vol->cache.unpin(disk_addr >> PAGE_SHIFT, pages_read);

    return pages_read;
}

bool btrfs_filesystem::flush_tree(const chunk& c, uint64_t addr) {
    unsigned int pages_per_tree = sb->nodesize >> PAGE_SHIFT;

    if (c.ci.type & BTRFS_BLOCK_GROUP_RAID0) {
        print("RAID0 not supported\n");
        return false;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID10) {
        print("RAID10 not supported\n");
        return false;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID5) {
        print("RAID5 not supported\n");
        return false;
    } else if (c.ci.type & BTRFS_BLOCK_GROUP_RAID6) {
        print("RAID6 not supported\n");
        return false;
    }

    // chunk is SINGLE, DUP, RAID1, RAID1C3, or RAID1C4

    // FIXME - handle missing devices

    // copy into page cache of volumes

    cache.pin(addr >> PAGE_SHIFT, pages_per_tree, nullptr);

    auto* first_page = cache.find_page_cache_entry(addr >> PAGE_SHIFT);
    if (!first_page) {
        print("cache.find_page_cache_entry returned NULL\n");
        return false;
    }

    // set WRITTEN flag

    auto& th = *(btrfs_header*)get_phys_address(*first_page);
    th.flags |= HEADER_FLAG_WRITTEN;

    // calculate hash

    {
        auto ent = first_page;
        uint32_t crc32 = 0xffffffff;

        for (unsigned j = 0; j < pages_per_tree; j++) {
            span<const uint8_t> sp;

            if (j == 0) {
                sp = span((const uint8_t*)get_phys_address(*ent) + offsetof(btrfs_header, fsid),
                          PAGE_SIZE - offsetof(btrfs_header, fsid));
            } else
                sp = span((const uint8_t*)get_phys_address(*ent), PAGE_SIZE);

            crc32 = calc_crc32c(crc32, sp);

            if (j != pages_per_tree - 1) {
                lock_guard lock(cache.cache_lock);
                ent = cache.next_page_cache_entry(*ent);
            }
        }

        *(uint32_t*)&th = ~crc32;
    }

    for (unsigned int i = 0; i < c.ci.num_stripes; i++) {
        auto ent = first_page;
        uint64_t disk_addr = addr - c.offset + c.ci.stripe[i].offset;

        for (unsigned j = 0; j < pages_per_tree; j++) {
            assert(ent->tree.key == (addr >> PAGE_SHIFT) + j);

            if (ent->dirty) {
                if (!vol->cache.write_pages(disk_addr >> PAGE_SHIFT, 1,
                    span((uint8_t*)get_phys_address(*ent), PAGE_SIZE))) {
                    print("volume::write_pages failed\n");
                    cache.unpin(addr >> PAGE_SHIFT, pages_per_tree);
                    return false;
                }
            }

            disk_addr += PAGE_SIZE;

            if (j != pages_per_tree - 1) {
                lock_guard lock(cache.cache_lock);
                ent = cache.next_page_cache_entry(*ent);
            }
        }
    }

    auto ent = first_page;
    for (unsigned j = 0; j < pages_per_tree; j++) {
        ent->dirty = 0;

        if (j != pages_per_tree - 1) {
            lock_guard lock(cache.cache_lock);
            ent = cache.next_page_cache_entry(*ent);
        }
    }

    cache.unpin(addr >> PAGE_SHIFT, pages_per_tree);

    return true;
}

size_t btrfs_filesystem::flush_cache(uint64_t page_num, uint64_t num_pages) {
    unsigned int pages_per_tree = sb->nodesize >> PAGE_SHIFT;
    uint64_t addr = page_num << PAGE_SHIFT;

    if (page_num % pages_per_tree) // not aligned to tree
        return 0;

    // find chunk

    auto pc = find_chunk(*this, addr);

    if (!pc) {
        print("Could not find chunk for logical address {:x}\n", addr);
        return 0;
    }

    auto& c = *pc;
    uint64_t pages_written = 0;

    while (addr < c.ci.length) {
        if (!flush_tree(c, addr))
            break;

        addr += sb->nodesize;
        pages_written += pages_per_tree;

        if (pages_written >= num_pages)
            break;
    }

    if (pages_written == 0)
        return pages_written;

    uint64_t pages_to_flush = pages_written;
    for (unsigned int i = 0; i < c.ci.num_stripes; i++) {
        uint64_t disk_addr = (page_num << PAGE_SHIFT) - c.offset + c.ci.stripe[i].offset;
        uint64_t first_page = disk_addr >> PAGE_SHIFT;

        while (true) {
            auto flushed = vol->flush_cache(first_page, pages_to_flush);

            if (flushed == 0) {
                print("volume::flush_cache failed\n");
                break;
            }

            if (pages_to_flush == flushed)
                break;

            first_page += flushed;
            pages_to_flush -= flushed;
        }
    }

    return pages_written;
}

void btrfs_filesystem::free_chunk_list() {
    while (!is_list_empty(chunk_list)) {
        auto& c = containing_record<chunk>(chunk_list.next);

        remove_entry_list(c.le);
        free(&c);
    }
}

void btrfs_filesystem::free_root_list() {
    while (!is_list_empty(root_list)) {
        auto& r = containing_record<root>(root_list.next);

        remove_entry_list(r.le);
        free(&r);
    }
}

static nt_status find_last_inode_number(btrfs_filesystem& fs, root& r) {
    btrfs_path p(fs.sb->nodesize);
    bool done = false;

    auto status = fs.find_item(r, {BTRFS_LAST_FREE_OBJECTID, key_type::INODE_ITEM, 0},
                               p, false);
    if (!success(status))
        return status;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (th.nritems == 0)
            break;

        if (p.slots[0] == th.nritems)
            p.slots[0] = th.nritems - 1;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.type == key_type::INODE_ITEM) {
            r.next_inode_number = k.objectid + 1;
            return nt_status::SUCCESS;
        }

        if (!fs.move_path_back(p))
            break;
    } while (true);

    if (!done)
        r.next_inode_number = BTRFS_FIRST_FREE_OBJECTID;

    return nt_status::SUCCESS;
}

bool btrfs_filesystem::load_roots() {
    list_entry new_root_list = { &new_root_list, &new_root_list };

    btrfs_path p(sb->nodesize);

    auto status = find_item(*root_root, {0, key_type::ROOT_ITEM, 0}, p, false);
    if (!success(status))
        return false;

    bool fail = false;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.type != key_type::ROOT_ITEM) {
            if (!move_path_forward(p))
                break;

            continue;
        }

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        if (data.size() < sizeof(btrfs_root_item)) {
            print("Root item truncated\n");
            fail = true;
            break;
        }

        auto rptr = new root;
        if (!rptr) {
            print("new failed\n");
            fail = true;
            break;
        }

        auto& r = *rptr;

        r.id = k.objectid;
        memcpy(&r.ri, data.data(), sizeof(btrfs_root_item));
        insert_tail_list(new_root_list, r.le);

        if (r.id == BTRFS_EXTENT_TREE_OBJECTID)
            extent_root = &r;
        else if (r.id == BTRFS_DEV_TREE_OBJECTID)
            dev_root = &r;
        else if (r.id == BTRFS_FS_TREE_OBJECTID)
            fs_root = &r;
        else if (r.id == BTRFS_CSUM_TREE_OBJECTID)
            csum_root = &r;
        else if (r.id == BTRFS_FREE_SPACE_TREE_OBJECTID)
            free_space_root = &r;
        else if (sb->compat_ro_flags & BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE &&
                 r.id == BTRFS_BLOCK_GROUP_TREE_OBJECTID) {
            block_group_root = &r;
        }

        if (!move_path_forward(p))
            break;
    } while (true);

    if (!extent_root) {
        print("no extent root\n");
        fail = true;
    } else if (!dev_root) {
        print("no dev root\n");
        fail = true;
    } else if (!fs_root) {
        print("no fs root\n");
        fail = true;
    } else if (!csum_root) {
        print("no csum root\n");
        fail = true;
    } else if (!free_space_root) {
        print("no free space root\n");
        fail = true;
    } else if (!block_group_root && sb->compat_ro_flags & BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE) {
        print("no block group root\n");
        fail = true;
    }

    if (fail) {
        swap_lists(root_list, new_root_list);
        free_root_list();
        swap_lists(root_list, new_root_list);
        return false;
    }

    if (!(sb->compat_ro_flags & BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE))
        block_group_root = extent_root;

    remove_entry_list(root_root->le);
    insert_head_list(new_root_list, root_root->le);

    remove_entry_list(chunk_root->le);
    insert_head_list(new_root_list, chunk_root->le);

    swap_lists(root_list, new_root_list);

    return true;
}

btrfs_inode::btrfs_inode(btrfs_filesystem& fs, uint64_t subvol, uint64_t inode, enum btrfs_inode_type inode_type) : fs(fs), inode(inode) {
    r = fs.find_root(subvol);
    if (!r)
        return;

    switch (inode_type) {
        case btrfs_inode_type::file:
            this->inode_type = inode_type::file;
            break;

        case btrfs_inode_type::directory:
            this->inode_type = inode_type::directory;
            break;

        case btrfs_inode_type::chardev:
            this->inode_type = inode_type::chardev;
            break;

        case btrfs_inode_type::blockdev:
            this->inode_type = inode_type::blockdev;
            break;

        case btrfs_inode_type::fifo:
            this->inode_type = inode_type::fifo;
            break;

        case btrfs_inode_type::socket:
            this->inode_type = inode_type::socket;
            break;

        case btrfs_inode_type::symlink:
            this->inode_type = inode_type::symlink;
            break;

        case btrfs_inode_type::ea:
            this->inode_type = inode_type::ea;
            break;

        default:
            this->inode_type = inode_type::unknown;
            break;
    }
}

bool btrfs_inode::load_inode() {
    nt_status status;
    bool found_inode_item = false;
    btrfs_path p(fs.sb->nodesize);

    status = fs.find_item(*r, {inode, key_type::INODE_ITEM, 0}, p, false);
    if (!success(status)) // FIXME - return this somehow
        return false;

    do {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.objectid != inode)
            break;

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        switch (k.type) {
            case key_type::INODE_ITEM:
                if (data.size() < sizeof(btrfs_inode_item)) {
                    print("INODE_ITEM was short\n");
                    break;
                }

                memcpy(&ii, data.data(), min(data.size(), sizeof(btrfs_inode_item)));
                found_inode_item = true;
            break;

            case key_type::EXTENT_DATA: {
                auto& ed = *(btrfs_file_extent_item*)data.data();

                if (data.size() < offsetof(btrfs_file_extent_item, data)) {
                    print("EXTENT_DATA was short\n");
                    break;
                }

                if (ed.compression != 0) {
                    print("Compression not supported\n");
                    break;
                }

                if (ed.encryption != 0) {
                    print("Encryption not supported\n");
                    break;
                }

                if (ed.other_encoding != 0) {
                    print("Other encodings not supported\n");
                    break;
                }

                if (ed.type == btrfs_extent_type::regular || ed.type == btrfs_extent_type::prealloc) {
                    if (data.size() < offsetof(btrfs_file_extent_item, data) + sizeof(btrfs_file_extent_item2)) {
                        print("EXTENT_DATA was short\n");
                        break;
                    }
                } else if (ed.type == btrfs_extent_type::inline_extent) {
                    if (data.size() < offsetof(btrfs_file_extent_item, data) + ed.ram_bytes) {
                        print("EXTENT_DATA was short\n");
                        break;
                    }
                } else {
                    print("Invalid extent type\n");
                    break;
                }

                if (ed.type == btrfs_extent_type::regular) {
                    auto& ed2 = *(btrfs_file_extent_item2*)ed.data;

                    if (ed2.disk_bytenr == 0 || ed2.disk_num_bytes == 0) // skip sparse extents
                        break;
                }

                auto ieptr = (inode_extent*)malloc(offsetof(inode_extent, ed) + data.size());
                if (!ieptr) {
                    print("malloc failed\n");
                    break;
                }

                auto& ie = *ieptr;

                ie.offset = k.offset;
                memcpy(&ie.ed, data.data(), data.size());
                insert_tail_list(extents, ie.le);

                break;
            }

            default:
                break;
        }

        if (!fs.move_path_forward(p))
            break;
    } while (true);

    if (!found_inode_item) {
        print("INODE_ITEM for subvol {:x}, inode {:x} not found.\n", r->id, inode);
        return false;
    }

    file_size = ii.size;

    if (this->inode_type == inode_type::unknown) {
        switch ((ii.mode & 0xf000) >> 12) {
            case 1:
                this->inode_type = inode_type::fifo;
                break;

            case 2:
                this->inode_type = inode_type::chardev;
                break;

            case 4:
                this->inode_type = inode_type::directory;
                break;

            case 6:
                this->inode_type = inode_type::blockdev;
                break;

            case 8:
                this->inode_type = inode_type::file;
                break;

            case 0xa:
                this->inode_type = inode_type::symlink;
                break;

            case 0xc:
                this->inode_type = inode_type::socket;
                break;

            default:
                break;
        }
    }

    return true;
}

btrfs_inode::~btrfs_inode() {
    while (!is_list_empty(extents)) {
        auto& ie = containing_record<inode_extent>(extents.next);

        remove_entry_list(ie.le);
        free(&ie);
    }
}

void btrfs_inode::release() {
    {
        lock_guard lock(fs.inode_lock);

        if (--refcount != 0)
            return;

        remove_entry_list(le);
    }

    delete this;
}

root* btrfs_filesystem::find_root(uint64_t root_num) {
    auto le = root_list.next;
    while (le != &root_list) {
        auto& r = containing_record<root>(le);

        if (r.id == root_num)
            return &r;

        le = le->next;
    }

    return nullptr;
}

static nt_status load_space(btrfs_filesystem& fs, chunk& c) {
    auto& r = *fs.free_space_root;
    auto search_key = btrfs_key{c.offset, key_type::FREE_SPACE_INFO, c.ci.length};
    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(r, search_key, p, false);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems) {
            print("Could not find free_space_info for chunk {:x}.\n", c.offset);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k != search_key) {
            print("Could not find free_space_info for chunk {:x}.\n", c.offset);
            return nt_status::DISK_CORRUPT_ERROR;
        }
    }

    do {
        if (!fs.move_path_forward(p))
            break;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.objectid >= c.offset + c.ci.length)
            break;

        switch (k.type) {
            case key_type::FREE_SPACE_EXTENT: {
                auto s = (chunk_space*)malloc(sizeof(chunk_space));

                // FIXME - these should be in some sort of tree rather than a linked list

                s->offset = k.objectid;
                s->length = k.offset;
                insert_tail_list(c.space_list, s->le);

                break;
            }

            case key_type::FREE_SPACE_BITMAP:
                print("FIXME - support free space bitmaps\n"); // FIXME
                return nt_status::INTERNAL_ERROR;
            break;

            default:
                break;
        }
    } while (true);

    c.space_loaded = true;

    return nt_status::SUCCESS;
}

static nt_status allocate_metadata_in_chunk(btrfs_filesystem& fs, chunk& c,
                                            uint64_t& addr) {
    if (!c.space_loaded) {
        auto status = load_space(fs, c);
        if (!success(status))
            return status;
    }

    // avoid overwriting previous metadata by following on from last allocation,
    // wherever possible
    if (c.last_allocation_end != 0) {
        for (auto le = c.space_list.next; le != &c.space_list; le = le->next) {
            auto& s = containing_record<chunk_space>(le);

            if (s.offset + s.length < c.last_allocation_end)
                continue;

            if (s.offset <= c.last_allocation_end && s.offset + s.length >= c.last_allocation_end + fs.sb->nodesize) {
                if (s.offset == c.last_allocation_end && s.length == fs.sb->nodesize) {
                    // remove whole thing
                    remove_entry_list(s.le);
                    free(&s);
                } else if (s.offset == c.last_allocation_end) {
                    // remove beginning
                    s.offset += fs.sb->nodesize;
                    s.length -= fs.sb->nodesize;
                } else if (s.offset + s.length == c.last_allocation_end + fs.sb->nodesize) {
                    // remove end
                    s.length -= fs.sb->nodesize;
                } else {
                    // remove middle

                    auto s2 = new chunk_space;
                    if (!s2)
                        return nt_status::INSUFFICIENT_RESOURCES;

                    s2->offset = c.last_allocation_end + fs.sb->nodesize;
                    s2->length = s.offset + s.length - s2->offset;
                    insert_head_list(s.le, s2->le);

                    s.length = c.last_allocation_end - s.offset;
                }

                addr = c.last_allocation_end;
                c.last_allocation_end = addr + fs.sb->nodesize;

                return nt_status::SUCCESS;
            }

            if (s.length < fs.sb->nodesize)
                continue;

            addr = s.offset;

            if (s.length == fs.sb->nodesize) {
                remove_entry_list(s.le);
                free(&s);
            } else {
                s.offset += fs.sb->nodesize;
                s.length -= fs.sb->nodesize;
            }

            c.last_allocation_end = addr + fs.sb->nodesize;

            return nt_status::SUCCESS;
        }
    }

    for (auto le = c.space_list.next; le != &c.space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.length >= fs.sb->nodesize) {
            addr = s.offset;

            if (s.length == fs.sb->nodesize) {
                remove_entry_list(s.le);
                free(&s);
            } else {
                s.offset += fs.sb->nodesize;
                s.length -= fs.sb->nodesize;
            }

            c.last_allocation_end = addr + fs.sb->nodesize;

            return nt_status::SUCCESS;
        }
    }

    return nt_status::DISK_FULL;
}

static nt_status allocate_metadata(btrfs_filesystem& fs, uint64_t& addr) {
    nt_status status;

    for (auto le = fs.chunk_list.next; le != &fs.chunk_list; le = le->next) {
        auto& c = containing_record<chunk>(le);

        // FIXME - skip if chunk is readonly

        if (!(c.ci.type & BTRFS_BLOCK_GROUP_METADATA))
            continue;

        // skip unsupported RAID types
        if (c.ci.type & (BTRFS_BLOCK_GROUP_RAID0 | BTRFS_BLOCK_GROUP_RAID10 | BTRFS_BLOCK_GROUP_RAID5 | BTRFS_BLOCK_GROUP_RAID6))
            continue;

        status = allocate_metadata_in_chunk(fs, c, addr);
        if (status != nt_status::DISK_FULL)
            return status;
    }

    // FIXME - if nothing found, try to allocate new chunk

    return nt_status::DISK_FULL;
}

static void add_to_space_list(chunk& c, uint64_t addr, uint64_t size) {
    for (auto le = c.space_list.next; le != &c.space_list; le = le->next) {
        auto& s = containing_record<chunk_space>(le);

        if (s.offset >= addr + size) {
            if (s.offset == addr + size) { // move beginning backwards
                s.length += size;
                s.offset = addr;

                while (s.le.prev != &c.space_list) {
                    auto& prev = containing_record<chunk_space>(s.le.prev);

                    if (prev.offset + prev.length == s.offset) {
                        s.length += prev.length;
                        s.offset = prev.offset;
                        remove_entry_list(prev.le);
                        delete &prev;
                    } else
                        break;
                }

                return;
            }

            // insert at very beginning
            if (s.le.prev == &c.space_list) {
                auto cs = new chunk_space;
                if (!cs)
                    return; // FIXME - return nt_status::INSUFFICIENT_RESOURCES

                cs->offset = addr;
                cs->length = size;
                insert_head_list(c.space_list, cs->le);
                return;
            }

            auto& prev = containing_record<chunk_space>(s.le.prev);

            // extend previous, and bridge if we can
            if (prev.offset + prev.length == addr) {
                prev.length += size;

                if (prev.offset + prev.length == s.offset) {
                    prev.length += s.length;
                    remove_entry_list(s.le);
                    delete &s;
                }

                return;
            }

            // insert new entry
            auto cs = new chunk_space;
            if (!cs)
                return; // FIXME - return nt_status::INSUFFICIENT_RESOURCES

            cs->offset = addr;
            cs->length = size;
            insert_head_list(prev.le, cs->le);

            return;
        }
    }

    // add at very end

    auto cs = new chunk_space;
    if (!cs)
        return; // FIXME - return nt_status::INSUFFICIENT_RESOURCES

    cs->offset = addr;
    cs->length = size;
    insert_tail_list(c.space_list, cs->le);
}

static void handle_pending_space(chunk& c) {
    while (!is_list_empty(c.pending_space_list)) {
        auto& cs = containing_record<chunk_space>(c.pending_space_list.next);

        add_to_space_list(c, cs.offset, cs.length);

        remove_entry_list(cs.le);
        delete &cs;
    }
}

static nt_status add_metadata_change(btrfs_filesystem& fs, uint64_t address,
                                     uint8_t level, uint64_t root, bool remove) {
    auto mc = (metadata_change*)malloc(sizeof(metadata_change));
    if (!mc)
        return nt_status::INSUFFICIENT_RESOURCES;

    mc->address = address;
    mc->level = level;
    mc->root = root;
    mc->remove = remove;
    insert_tail_list(fs.metadata_changes, mc->le);

    return nt_status::SUCCESS;
}

static nt_status cow_tree(btrfs_filesystem& fs, root& r, btrfs_path& p) {
    // FIXME - updating free space tree

    for (uint8_t i = 0; i < BTRFS_MAX_LEVEL; i++) {
        uint64_t addr;

        if (!p.data[i])
            break;

        auto& th = *(btrfs_header*)p.data[i];

        if (th.flags & HEADER_FLAG_WRITTEN) {
            if (auto status = allocate_metadata(fs, addr); !success(status))
                return status;

            auto old_addr = th.bytenr;

            th.flags &= ~HEADER_FLAG_WRITTEN;
            th.bytenr = addr;
            th.generation = fs.sb->generation;

            if (!fs.cache.write(span(p.data[i], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;

            if (i == BTRFS_MAX_LEVEL - 1 || !p.data[i + 1]) { // top
                r.ri.bytenr = addr;
                r.ri.generation = fs.sb->generation;
                r.ri.generation_v2 = fs.sb->generation;

                if (r.id != BTRFS_ROOT_TREE_OBJECTID && !r.is_dirty) {
                    insert_tail_list(fs.dirty_root_list, r.dirty_le);
                    r.is_dirty = true;
                }
            }

            if (auto status = add_metadata_change(fs, addr, i, r.id, false); !success(status))
                return status;

            if (auto status = add_metadata_change(fs, old_addr, i, r.id, true); !success(status))
                return status;
        }

        if (i > 0) {
            auto& prev_th = *(btrfs_header*)p.data[i - 1];
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            if (items[p.slots[i]].blockptr == prev_th.bytenr)
                break;

            items[p.slots[i]].blockptr = prev_th.bytenr;
            items[p.slots[i]].generation = fs.sb->generation;

            if (!fs.cache.write(span(p.data[i], fs.sb->nodesize), th.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    return nt_status::SUCCESS;
}

btrfs_path::~btrfs_path() {
    for (unsigned int i = 0; i < BTRFS_MAX_LEVEL; i++) {
        if (data[i])
            free_folio(phys_memory_pfn(data[i]) << PAGE_SHIFT);
    }
}

nt_status btrfs_filesystem::find_item(root& r, const btrfs_key& k, btrfs_path& p,
                                      bool cow) {
    uint64_t addr = r.ri.bytenr;
    uint8_t level = r.ri.level;
    nt_status status;

    // FIXME - error if level >= BTRFS_MAX_LEVEL

    for (unsigned int i = 0; i <= level; i++) {
        auto pa = allocate_folio(node_folio_order);
        if (!pa)
            return nt_status::INSUFFICIENT_RESOURCES;

        p.data[i] = (uint8_t*)phys_address(pa);
    }

    do {
        status = read_metadata(addr, span(p.data[level], sb->nodesize));
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[level];

        if (th.level != level) {
            print("Tree level was incorrect ({:x}, expected {:x})\n", th.level, level);
            return nt_status::DISK_CORRUPT_ERROR;
        }

        if (th.level == 0) {
            auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            p.slots[level] = (uint32_t)items.size();

            for (uint32_t i = 0; i < items.size(); i++) {
                if (items[i].key >= k) {
                    p.slots[level] = i;
                    break;
                }
            }

            break;
        } else {
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            p.slots[level] = th.nritems - 1;

            for (uint32_t i = 1; i < items.size(); i++) {
                if (items[i].key == k) {
                    p.slots[level] = i;
                    break;
                } else if (items[i].key > k) {
                    p.slots[level] = i - 1;
                    break;
                }
            }

            addr = items[p.slots[level]].blockptr;
            level--;
        }
    } while (true);

    if (cow) {
        status = cow_tree(*this, r, p);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static unsigned int find_midpoint(const btrfs_header& th, size_t total,
                                  size_t& sum) {
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    sum = 0;

    for (unsigned int i = 0; i < th.nritems; i++) {
        if (sum + sizeof(btrfs_item) + items[i].size > total / 2)
            return i;

        sum += sizeof(btrfs_item) + items[i].size;
    }

    return th.nritems - 1;
}

static nt_status split_internal_tree(btrfs_filesystem& fs, btrfs_path& p,
                                     root& r, uint8_t level) {
    if (level >= BTRFS_MAX_LEVEL - 2)
        return nt_status::INTERNAL_ERROR;

    auto& th = *(btrfs_header*)p.data[level];
    unsigned int split_point = th.nritems / 2;
    uint64_t new_addr;
    nt_status status;

    auto new_tree = (uint8_t*)phys_address(allocate_folio(fs.node_folio_order));
    if (!new_tree)
        return nt_status::INSUFFICIENT_RESOURCES;

    status = allocate_metadata(fs, new_addr);
    if (!success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    auto& th2 = *(btrfs_header*)new_tree;

    memcpy(&th2, &th, sizeof(btrfs_header));
    th2.bytenr = new_addr;
    th2.flags &= ~HEADER_FLAG_WRITTEN;
    th2.nritems = th.nritems - split_point;

    auto* items = (btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header));
    auto* items2 = (btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header));

    memcpy(items2, &items[split_point], th2.nritems * sizeof(btrfs_key_ptr));
    th.nritems = split_point;

    status = add_metadata_change(fs, new_addr, level, th.owner, false);
    if (!success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    if (!fs.cache.write(span((uint8_t*)&th, fs.sb->nodesize), th.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!fs.cache.write(span((uint8_t*)&th2, fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!p.data[level + 1]) {
        status = add_new_level(fs, p, r, level + 1);
        if (!success(status)) {
            free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
            return status;
        }
    }

    p.slots[level + 1]++;
    status = insert_internal_node(fs, p, r, level + 1, items2[0].key, new_addr);
    if (!success(status)) {
        p.slots[level + 1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    if (p.slots[level] < split_point) {
        p.slots[level + 1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
    } else {
        p.slots[level] -= split_point;
        free_folio(phys_memory_pfn(p.data[level]) << PAGE_SHIFT);
        p.data[level] = new_tree;
    }

    return nt_status::SUCCESS;
}

static nt_status insert_internal_node(btrfs_filesystem& fs, btrfs_path& p,
                                      root& r, uint8_t level, const btrfs_key& k,
                                      uint64_t address) {
    nt_status status;
    size_t max_items = (fs.sb->nodesize - sizeof(btrfs_header)) / sizeof(btrfs_key_ptr);

    {
        auto& th = *(btrfs_header*)p.data[level];

        if (th.nritems == max_items) {
            status = split_internal_tree(fs, p, r, level);
            if (!success(status))
                return status;
        }
    }

    auto& th = *(btrfs_header*)p.data[level];

    auto* items = (btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header));

    memmove(&items[p.slots[level] + 1], &items[p.slots[level]],
            (th.nritems - p.slots[level]) * sizeof(btrfs_key_ptr));

    items[p.slots[level]].key = k;
    items[p.slots[level]].blockptr = address;
    items[p.slots[level]].generation = fs.sb->generation;

    th.nritems++;

    if (!fs.cache.write(span(p.data[level], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status add_new_level(btrfs_filesystem& fs, btrfs_path& p, root& r,
                               uint8_t level) {
    uint64_t new_addr;
    btrfs_key first_key;

    auto new_tree = (uint8_t*)phys_address(allocate_folio(fs.node_folio_order));
    if (!new_tree)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (auto status = allocate_metadata(fs, new_addr); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    memcpy(new_tree, p.data[level - 1], sizeof(btrfs_header));

    auto& th = *(btrfs_header*)p.data[level - 1];

    if (level - 1 == 0) {
        auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

        first_key = items[0].key;
    } else {
        auto* items = (btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header));

        first_key = items[0].key;
    }

    auto& th2 = *(btrfs_header*)new_tree;
    th2.bytenr = new_addr;
    th2.flags &= ~HEADER_FLAG_WRITTEN;
    th2.level = level;
    th2.nritems = 1;

    if (auto status = add_metadata_change(fs, new_addr, level, th.owner, false); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    auto* items2 = (btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header));

    items2[0].key = first_key;
    items2[0].blockptr = th.bytenr;
    items2[0].generation = fs.sb->generation;

    memset(&items2[1], 0, fs.sb->nodesize - sizeof(btrfs_header) - sizeof(btrfs_key_ptr));

    if (!fs.cache.write(span(new_tree, fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    p.data[level] = new_tree;
    p.slots[level] = 0;

    r.ri.bytenr = new_addr;
    r.ri.level = level;
    r.ri.generation = fs.sb->generation;
    r.ri.generation_v2 = fs.sb->generation;

    if (r.id != BTRFS_ROOT_TREE_OBJECTID && !r.is_dirty) {
        insert_tail_list(fs.dirty_root_list, r.dirty_le);
        r.is_dirty = true;
    }

    return nt_status::SUCCESS;
}

static nt_status split_tree_at(btrfs_filesystem& fs, btrfs_path& p,
                               root& r, unsigned int split_point) {
    uint64_t new_addr;
    nt_status status;

    auto new_tree = (uint8_t*)phys_address(allocate_folio(fs.node_folio_order));
    if (!new_tree)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (auto status = allocate_metadata(fs, new_addr); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));
    size_t to_copy = 0, total_data = 0;

    for (unsigned int i = 0; i < th.nritems; i++) {
        total_data += items[i].size;

        if (i >= split_point)
            to_copy += items[i].size;
    }

    memcpy(new_tree, p.data[0], sizeof(btrfs_header));

    auto& th2 = *(btrfs_header*)new_tree;
    th2.bytenr = new_addr;
    th2.flags &= ~HEADER_FLAG_WRITTEN;
    th2.nritems = th.nritems - split_point;

    // copy btrfs_item items
    auto* items2 = (btrfs_item*)((uint8_t*)&th2 + sizeof(btrfs_header));
    memcpy(items2, &items[split_point], sizeof(btrfs_item) * (th.nritems - split_point));

    // move trailing entries to new tree
    memcpy(new_tree + fs.sb->nodesize - to_copy,
           p.data[0] + fs.sb->nodesize - total_data, to_copy);

    for (unsigned int i = 0; i < th2.nritems; i++) {
        items2[i].offset += (uint32_t)(total_data - to_copy);
    }

    th.nritems = split_point;

    memset(new_tree + sizeof(btrfs_header) + (th2.nritems * sizeof(btrfs_item)),
           0, fs.sb->nodesize - to_copy - sizeof(btrfs_header) - (th2.nritems * sizeof(btrfs_item)));

    if (auto status = add_metadata_change(fs, new_addr, 0, th.owner, false); !success(status)) {
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    // write both trees into cache

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!fs.cache.write(span(new_tree, fs.sb->nodesize), th2.bytenr)) { // FIXME - return status
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return nt_status::INTERNAL_ERROR;
    }

    if (!p.data[1]) {
        status = add_new_level(fs, p, r, 1);
        if (!success(status)) {
            free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
            return status;
        }
    }

    p.slots[1]++;
    status = insert_internal_node(fs, p, r, 1, items2[0].key, new_addr);
    if (!success(status)) {
        p.slots[1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
        return status;
    }

    if (p.slots[0] < split_point || split_point == 0) {
        p.slots[1]--;
        free_folio(phys_memory_pfn(new_tree) << PAGE_SHIFT);
    } else {
        p.slots[0] -= split_point;
        free_folio(phys_memory_pfn(p.data[0]) << PAGE_SHIFT);
        p.data[0] = new_tree;
    }

    return nt_status::SUCCESS;
}

static nt_status split_tree(btrfs_filesystem& fs, btrfs_path& p, root& r,
                            size_t size_used, size_t space_needed) {
    // FIXME - first try to push left and right

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    size_t sum;
    auto split_point = find_midpoint(th, size_used, sum);
    bool need_double_split = false;

    if (p.slots[0] >= split_point)
        sum = size_used - sum;

    if (fs.sb->nodesize - sizeof(btrfs_header) - sum < space_needed) {
        if (p.slots[0] < split_point) {
            while (split_point > 0) {
                sum += sizeof(btrfs_item) + items[split_point - 1].size;
                split_point--;

                if (fs.sb->nodesize - sizeof(btrfs_header) - sum >= space_needed)
                    break;
            }

            if (split_point == 0)
                need_double_split = true;
        } else {
            while (split_point < th.nritems) {
                sum += sizeof(btrfs_item) + items[split_point].size;
                split_point++;

                if (fs.sb->nodesize - sizeof(btrfs_header) - sum >= space_needed)
                    break;
            }

            if (split_point == th.nritems)
                need_double_split = true;
        }
    }

    if (!need_double_split)
        return split_tree_at(fs, p, r, split_point);

    if (p.slots[0] != 0) {
        if (auto status = split_tree_at(fs, p, r, p.slots[0]); !success(status))
            return status;
    }

    auto num_items = ((btrfs_header*)p.data[0])->nritems;

    if (num_items != 0 && p.slots[0] < num_items - 1) {
        if (auto status = split_tree_at(fs, p, r, p.slots[0]); !success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static nt_status insert_item(btrfs_filesystem& fs, root& r, const btrfs_key& k,
                             span<const uint8_t> data) {
    btrfs_path p(fs.sb->nodesize);
    nt_status status;

    // check if data too big for any tree
    if (data.size() > fs.sb->nodesize - sizeof(btrfs_header) - sizeof(btrfs_item))
        return nt_status::INTERNAL_ERROR;

    status = fs.find_item(r, k, p, true);
    if (!success(status))
        return status;

    {
        auto& th = *(btrfs_header*)p.data[0];
        auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

        if (p.slots[0] < th.nritems && items[p.slots[0]].key == k)
            return nt_status::OBJECT_NAME_COLLISION;

        unsigned int data_size = 0;

        for (unsigned int i = 0; i < th.nritems; i++) {
            data_size += items[i].size;
        }

        unsigned int size_used = data_size + (unsigned int)(th.nritems * sizeof(btrfs_item));

        if (size_used + sizeof(btrfs_item) + data.size() > fs.sb->nodesize - sizeof(btrfs_header)) {
            status = split_tree(fs, p, r, size_used, sizeof(btrfs_item) + data.size());
            if (!success(status))
                return status;
        }
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    // insert new btrfs_item

    if (p.slots[0] < th.nritems) {
        memmove(&items[p.slots[0] + 1], &items[p.slots[0]],
                (th.nritems - p.slots[0]) * sizeof(btrfs_item));
    }

    th.nritems++;

    items[p.slots[0]].key = k;

    if (!data.empty() && p.slots[0] != th.nritems - 1) {
        unsigned int to_move = 0;

        // move data around

        uint32_t off = sizeof(btrfs_header) + items[th.nritems - 1].offset;

        for (unsigned int i = p.slots[0] + 1; i < th.nritems; i++) {
            to_move += items[i].size;
            items[i].offset -= (uint32_t)data.size();
        }

        assert(off >= data.size() + sizeof(btrfs_header));

        memmove(p.data[0] + off - data.size(), p.data[0] + off, to_move);
    }

    if (p.slots[0] == 0)
        items[p.slots[0]].offset = (uint32_t)(fs.sb->nodesize - sizeof(btrfs_header) - data.size());
    else
        items[p.slots[0]].offset = (uint32_t)(items[p.slots[0] - 1].offset - data.size());

    items[p.slots[0]].size = (uint32_t)data.size();

    memcpy(p.data[0] + sizeof(btrfs_header) + items[p.slots[0]].offset,
           data.data(), data.size());

    // update parents if new first item

    if (p.slots[0] == 0 && p.data[1]) {
        for (uint8_t i = 0; i < BTRFS_MAX_LEVEL - 1; i++) {
            if (p.slots[i] != 0 || !p.data[i + 1])
                break;

            auto& th2 = *(btrfs_header*)p.data[i + 1];
            auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

            items2[p.slots[i + 1]].key = items[0].key;

            if (!fs.cache.write(span(p.data[i + 1], fs.sb->nodesize), th2.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::remove_item2(btrfs_path& p) {
    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    // move data

    if (items[p.slots[0]].size != 0) {
        unsigned int data_size = 0, after_item = 0;

        for (unsigned int i = 0; i < th.nritems; i++) {
            data_size += items[i].size;

            if (i > p.slots[0])
                after_item += items[i].size;
        }

        memmove(p.data[0] + sb->nodesize - data_size + items[p.slots[0]].size,
                p.data[0] + sb->nodesize - data_size,
                after_item);

        for (unsigned int i = p.slots[0] + 1; i < th.nritems; i++) {
            items[i].offset += items[p.slots[0]].size;
        }
    }

    // adjust items

    memmove(&items[p.slots[0]], &items[p.slots[0] + 1],
            sizeof(btrfs_item) * (th.nritems - p.slots[0] - 1));
    th.nritems--;

    // update parents if deleting first item

    if (p.slots[0] == 0 && p.data[1]) {
        for (uint8_t i = 0; i < BTRFS_MAX_LEVEL - 1; i++) {
            if (p.slots[i] != 0 || !p.data[i + 1])
                break;

            auto& th2 = *(btrfs_header*)p.data[i + 1];
            auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

            items2[p.slots[i + 1]].key = items[0].key;

            if (!cache.write(span(p.data[i + 1], sb->nodesize), th2.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    // FIXME - if nritems is now 0 and not top, remove entry in parent
    // FIXME - adjust levels if internal tree has only one entry
    // FIXME - merging trees

    // copy tree back into page cache and mark dirty

    if (!cache.write(span(p.data[0], sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::remove_item(root& r, const btrfs_key& k) {
    btrfs_path p(sb->nodesize);
    nt_status status;

    status = find_item(r, k, p, true);
    if (!success(status))
        return status;

    const auto& th = *(btrfs_header*)p.data[0];
    const auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    if (items[p.slots[0]].key != k || p.slots[0] >= th.nritems)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    return remove_item2(p);
}

nt_status btrfs_filesystem::change_key(btrfs_path& p, const btrfs_key& k) {
    auto& th = *(btrfs_header*)p.data[0];
    auto* items = (btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header));

    items[p.slots[0]].key = k;

    // update parents if changing first item

    if (p.slots[0] == 0 && p.data[1]) {
        for (uint8_t i = 0; i < BTRFS_MAX_LEVEL - 1; i++) {
            if (p.slots[i] != 0 || !p.data[i + 1])
                break;

            auto& th2 = *(btrfs_header*)p.data[i + 1];
            auto items2 = span((btrfs_key_ptr*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

            items2[p.slots[i + 1]].key = items[0].key;

            if (!cache.write(span(p.data[i + 1], sb->nodesize), th2.bytenr)) // FIXME - return status
                return nt_status::INTERNAL_ERROR;
        }
    }

    // copy tree back into page cache and mark dirty

    if (!cache.write(span(p.data[0], sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

bool btrfs_filesystem::move_path_back(btrfs_path& p) {
    if (p.slots[0] != 0) {
        p.slots[0]--;
        return true;
    }

    for (uint8_t i = 1; i < BTRFS_MAX_LEVEL; i++) {
        if (!p.data[i])
            return false;

        if (p.slots[i] == 0)
            continue;

        p.slots[i]--;

        for (auto j = (int8_t)i; j > 0; j--) {
            const auto& th = *(btrfs_header*)p.data[j];
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            auto status = read_metadata(items[p.slots[j]].blockptr, span(p.data[j - 1], sb->nodesize));
            if (!success(status)) // FIXME
                return false;

            const auto& th2 = *(btrfs_header*)p.data[j - 1];

            p.slots[j - 1] = th2.nritems - 1;
        }

        return true;
    }

    return false;
}

bool btrfs_filesystem::move_path_forward(btrfs_path& p) {
    if (p.slots[0] < ((btrfs_header*)p.data[0])->nritems - 1) {
        p.slots[0]++;
        return true;
    }

    for (uint8_t i = 1; i < BTRFS_MAX_LEVEL; i++) {
        if (!p.data[i])
            return false;

        if (p.slots[i] == ((btrfs_header*)p.data[i])->nritems - 1)
            continue;

        p.slots[i]++;

        for (auto j = (int8_t)i; j > 0; j--) {
            const auto& th = *(btrfs_header*)p.data[j];
            auto items = span((btrfs_key_ptr*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            auto status = read_metadata(items[p.slots[j]].blockptr, span(p.data[j - 1], sb->nodesize));
            if (!success(status)) // FIXME
                return false;

            p.slots[j - 1] = 0;
        }

        return true;
    }

    return false;
}

nt_status btrfs_inode::get_last_dir_index(uint64_t& ret) {
    nt_status status;
    btrfs_path p(fs.sb->nodesize);

    status = fs.find_item(*r, {inode, key_type::DIR_INDEX, 0xffffffffffffffff},
                          p, false);
    if (!success(status))
        return status;

    if (!fs.move_path_back(p)) {
        ret = 1;
        return nt_status::SUCCESS;
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (items[p.slots[0]].key.objectid != inode || items[p.slots[0]].key.type != key_type::DIR_INDEX) {
        ret = 1;
        return nt_status::SUCCESS;
    }

    ret = items[p.slots[0]].key.offset;

    return nt_status::SUCCESS;
}

static btrfs_timespec get_btrfs_time() {
    auto now = (uint64_t)system_time + system_time_offset;
    btrfs_timespec ret;

    ret.nsec = (uint32_t)((now % SYSTEM_TIME_RESOLUTION) * (1000000000 / SYSTEM_TIME_RESOLUTION));
    now /= SYSTEM_TIME_RESOLUTION;

    ret.sec = now - 11644473600;

    return ret;
}

static nt_status adjust_inode_size(btrfs_filesystem& fs, root& r, uint64_t inode,
                                   int64_t diff, const btrfs_timespec& cur_time) {
    btrfs_path p(fs.sb->nodesize);
    nt_status status;

    status = fs.find_item(r, {inode, key_type::INODE_ITEM, 0}, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key.objectid != inode || found_key.type != key_type::INODE_ITEM)
        return nt_status::DISK_CORRUPT_ERROR;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (it.size < sizeof(btrfs_inode_item))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& ii = *(btrfs_inode_item*)data.data();

    ii.size = (uint64_t)((int64_t)ii.size + diff);
    ii.sequence++;
    ii.ctime = cur_time;
    ii.mtime = cur_time;

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static bool validate_name(string_view name) {
    if (name.empty())
        return false;

    if (name == "." || name == "..")
        return false;

    if (name.size() >= 256)
        return false;

    for (auto c : name) {
        if (c == '\0' || c == '/' || c == '\\')
            return false;
    }

    return true;
}

nt_status btrfs_inode::create_child(string_view name, uint32_t create_options,
                                    optional<reference_wrapper<btrfs_inode>>& obj) {
    nt_status status;
    btrfs_inode_item ii;
    bool is_dir = create_options & FILE_DIRECTORY_FILE;
    uint64_t new_inode;

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    if (r->next_inode_number == 0) {
        status = find_last_inode_number(fs, *r);
        if (!success(status))
            return status;
    }

    new_inode = r->next_inode_number.fetch_add(1);

    auto cur_time = get_btrfs_time();

    // create INODE_ITEM

    memset(&ii, 0, sizeof(btrfs_inode_item));
    ii.generation = fs.sb->generation;
    ii.transid = fs.sb->generation;
    ii.nlink = 1;
    ii.uid = 0; // FIXME
    ii.gid = 0; // FIXME

    // FIXME - do we inherit mode from parent?
    if (is_dir)
        ii.mode = S_IFDIR | 0755;
    else
        ii.mode = S_IFREG | 0644;

    ii.sequence = 0; // FIXME?
    ii.ctime = cur_time;
    ii.mtime = cur_time;
    ii.otime = cur_time;

    status = insert_item(fs, *r, {new_inode, key_type::INODE_ITEM, 0},
                         span((uint8_t*)&ii, sizeof(ii)));
    if (!success(status)) {
        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;
        return status;
    }

    // create DIR_ITEM in parent

    auto hash = calc_crc32c(0xfffffffe, name);

    size_t di_size = offsetof(btrfs_dir_item, name) + name.size();
    auto di = (btrfs_dir_item*)malloc(di_size);
    if (!di)
        return nt_status::INSUFFICIENT_RESOURCES;

    di->location.objectid = new_inode;
    di->location.type = key_type::INODE_ITEM;
    di->location.offset = 0;
    di->transid = fs.sb->generation;
    di->data_len = 0;
    di->name_len = (uint16_t)name.size();
    di->type = is_dir ? btrfs_inode_type::directory : btrfs_inode_type::file;
    memcpy(di->name, name.data(), name.size());

    status = insert_item(fs, *r, {inode, key_type::DIR_ITEM, hash},
                         span((uint8_t*)di, di_size));

    // FIXME - handle DIR_ITEM hash collisions

    if (!success(status)) {
        free(di);

        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;

        return status;
    }

    // create DIR_INDEX in parent

    uint64_t index;

    status = get_last_dir_index(index);
    if (!success(status)) {
        free(di);
        return status;
    }

    index++;

    status = insert_item(fs, *r, {inode, key_type::DIR_INDEX, index},
                         span((uint8_t*)di, di_size));

    if (!success(status)) {
        free(di);

        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;

        return status;
    }

    free(di);

    // create INODE_REF

    size_t ir_size = offsetof(btrfs_inode_ref, name) + name.size();
    auto ir = (btrfs_inode_ref*)malloc(ir_size);
    if (!ir)
        return nt_status::INSUFFICIENT_RESOURCES;

    ir->index = index;
    ir->name_len = (uint16_t)name.size();
    memcpy(ir->name, name.data(), name.size());

    status = insert_item(fs, *r, {new_inode, key_type::INODE_REF, inode},
                         span((uint8_t*)ir, ir_size));

    if (!success(status)) {
        free(ir);

        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;

        return status;
    }

    free(ir);

    // adjust size of parent directory

    // FIXME - adjust file_size and inode_item in in-memory inode?

    status = adjust_inode_size(fs, *r, inode, (int64_t)(name.size() * 2), cur_time);
    if (!success(status))
        return status;

    // adjust root_item

    r->ri.ctime = cur_time;
    r->ri.ctransid = fs.sb->generation;

    // create btrfs_inode object and return

    auto bi = new btrfs_inode(fs, r->id, new_inode,
                              is_dir ? btrfs_inode_type::directory : btrfs_inode_type::file);

    memcpy(&bi->ii, &ii, sizeof(btrfs_inode_item));
    bi->file_size = 0;

    {
        lock_guard lock(fs.inode_lock);
        insert_tail_list(fs.inode_list, bi->le);
    }

    obj = *bi;

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::open_child(string_view name, uint32_t create_disposition,
                                  uint32_t create_options, unsigned int& create_result,
                                  optional<reference_wrapper<btrfs_inode>>& obj) {
    if (inode_type != inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    auto hash = calc_crc32c(0xfffffffe, name);

    optional<btrfs_key> dir_item_key;

    do {
        auto k = btrfs_key{inode, key_type::DIR_ITEM, hash};
        btrfs_path p(fs.sb->nodesize);

        auto status = fs.find_item(*r, k, p, false);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key != k)
            break;

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        while (data.size() >= offsetof(btrfs_dir_item, name)) {
            const auto& di = *(btrfs_dir_item*)data.data();

            if (data.size() < offsetof(btrfs_dir_item, name) + di.data_len + di.name_len)
                break;

            if (string_view(di.name, di.name_len) == name) {
                dir_item_key = di.location;
                break;
            }

            data = data.subspan(offsetof(btrfs_dir_item, name) + di.data_len + di.name_len,
                                data.size() - offsetof(btrfs_dir_item, name) - di.data_len - di.name_len);
        }
    } while (false);

    if (dir_item_key.has_value()) {
        switch (create_disposition) {
            case FILE_CREATE:
                return nt_status::OBJECT_NAME_COLLISION;

            case FILE_SUPERSEDE:
                print("FIXME - FILE_SUPERSEDE\n");
                return nt_status::NOT_SUPPORTED;

            case FILE_OVERWRITE:
                print("FIXME - FILE_OVERWRITE\n");
                return nt_status::NOT_SUPPORTED;

            case FILE_OVERWRITE_IF:
                print("FIXME - FILE_OVERWRITE_IF\n");
                return nt_status::NOT_SUPPORTED;

            case FILE_OPEN:
            case FILE_OPEN_IF:
                create_result = FILE_OPENED;
                break;
        }

        auto& v = *dir_item_key;

        if (v.type == key_type::ROOT_ITEM)
            return fs.open_inode(v.objectid, SUBVOL_ROOT_INODE, obj);
        else
            return fs.open_inode(r->id, v.objectid, obj);
    } else {
        if (create_disposition == FILE_OPEN || create_disposition == FILE_OVERWRITE) {
            create_result = FILE_DOES_NOT_EXIST;
            return nt_status::OBJECT_NAME_NOT_FOUND;
        }

        auto status = create_child(name, create_options, obj);

        if (!success(status))
            return status;

        create_result = FILE_CREATED;

        return status;
    }
}

nt_status btrfs_filesystem::open_inode(uint64_t subvol, uint64_t inode,
                                       optional<reference_wrapper<btrfs_inode>>& obj) {
    {
        lock_guard lock(inode_lock);

        auto le = inode_list.next;
        while (le != &inode_list) {
            auto& in = containing_record<btrfs_inode>(le);

            if (in.r->id == subvol && in.inode == inode) {
                in.acquire();
                obj = in;
                return nt_status::SUCCESS;
            }

            le = le->next;
        }
    }

    auto ino = new btrfs_inode(*this, subvol, inode, btrfs_inode_type::unknown);

    if (!ino) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!ino->load_inode()) {
        delete ino;
        return nt_status::INTERNAL_ERROR;
    }

    {
        optional<lock_guard<spinlock>> lock(inode_lock);

        auto le = inode_list.next;
        while (le != &inode_list) {
            auto& in = containing_record<btrfs_inode>(le);

            if (in.r->id == subvol && in.inode == inode) {
                in.acquire();
                lock.reset();

                delete ino;

                obj = in;
                return nt_status::SUCCESS;
            }

            le = le->next;
        }

        insert_tail_list(inode_list, ino->le);
    }

    obj = *ino;
    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::open(string_view fn, const optional<reference_wrapper<inode>>& root_dir,
                                 object_ref& obj_ref, uint32_t create_disposition,
                                 uint32_t create_options, unsigned int& create_result) {

    if (readonly && create_disposition != FILE_OPEN)
        return nt_status::MEDIA_WRITE_PROTECTED;

    if (!root_dir) {
        if (fn.empty() || fn[0] != '/') {
            print("Not an absolute path.\n");
            return nt_status::INVALID_PARAMETER;
        }

        fn.remove_prefix(1);

        if (fn.empty()) { // root
            root_inode->acquire();
            obj_ref = *root_inode;
            return nt_status::SUCCESS;
        }
    }

    // FIXME - do we need to make sure root_dir is definitely a btrfs inode?

    reference_wrapper<btrfs_inode> dirrw{!root_dir ? *root_inode : static_cast<btrfs_inode&>(root_dir->get())};
    dirrw.get().acquire();

    auto l = [&]() -> nt_status {
        // FIXME - die if anything in middle not directory

        while (true) {
            auto sl = fn.find("/");
            auto& dir = dirrw.get();
            bool last_bit = sl == string_view::npos;
            optional<reference_wrapper<btrfs_inode>> objrw;
            unsigned int result;

            // FIXME - .. and .

            auto status = dir.open_child(last_bit ? fn : string_view(fn.data(), sl),
                                         last_bit ? create_disposition : FILE_OPEN,
                                         create_options, last_bit ? create_result : result,
                                         objrw);
            if (!success(status)) {
                dir.release();

                if (!last_bit && status == nt_status::OBJECT_NAME_NOT_FOUND)
                    status = nt_status::OBJECT_PATH_NOT_FOUND;

                return status;
            }

            auto& obj = objrw->get();

            dir.release();

            if (last_bit) {
                obj_ref = obj;
                return nt_status::SUCCESS;
            }

            fn = string_view(fn.data() + sl + 1, fn.size() - sl - 1);
            dirrw = obj;
        }
    };

    // get trans_lock RO if FILE_OPEN, RW otherwise
    if (create_disposition == FILE_OPEN) {
        shared_lock lock(trans_lock);

        return l();
    } else {
        lock_guard lock(trans_lock);

        return l();
    }
}

size_t btrfs_inode::read_zeroes_into_cache(uint64_t page_num, uint64_t num_pages) {
    page_cache_entry* dest_p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);
        }

        increase_page_refcount(zero_pfn);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        dest_p->pfn = zero_pfn;
#pragma GCC diagnostic pop
    }

    return num_pages;
}

size_t btrfs_inode::read_inline_extent_into_cache(uint64_t page_num,
                                                  uint64_t num_pages,
                                                  const inode_extent& ie) {
    if (ie.ed.compression != 0) {
        print("FIXME - compression\n"); // FIXME
        return 0;
    }

    if (page_num >= bytes_to_pages(ie.ed.ram_bytes))
        return 0;

    num_pages = min(num_pages, (uint64_t)bytes_to_pages(ie.ed.ram_bytes) - page_num);

    if (num_pages == 0)
        return 0;

    page_cache_entry* dest_p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);

            if (dest_p->pfn == 0) {
                auto pa = allocate_folio(0);
                if (!pa) {
                    print("allocate_folio failed\n");
                    return i;
                }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop
            }
        }

        auto dest_va = get_phys_address(*dest_p);

        uint64_t offset = (page_num + i) << PAGE_SHIFT;
        auto to_copy = min((uint64_t)PAGE_SIZE, ie.ed.ram_bytes - offset);

        memcpy(dest_va, ie.ed.data + offset, to_copy);

        memset((uint8_t*)dest_va + to_copy, 0, PAGE_SIZE - to_copy);
    }

    return num_pages;
}

static nt_status check_csums(btrfs_inode& ino, uint64_t page_num, uint64_t addr,
                             uint64_t num_pages) {
    page_cache_entry* src_p;
    bool first = true;

    auto& fs = ino.fs;
    auto k = btrfs_key{BTRFS_EXTENT_CSUM_OBJECTID, key_type::EXTENT_CSUM, addr};

    do {
        btrfs_path p(fs.sb->nodesize);

        auto status = fs.find_item(*fs.csum_root, k, p, false);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] == 0 && items[p.slots[0]].key != k)
            return nt_status::OBJECT_NAME_NOT_FOUND;

        if (items[p.slots[0]].key != k)
            p.slots[0]--;

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key.objectid != BTRFS_EXTENT_CSUM_OBJECTID || found_key.type != key_type::EXTENT_CSUM)
        return nt_status::OBJECT_NAME_NOT_FOUND;

        auto data = span((uint32_t*)((const uint8_t*)&th + sizeof(btrfs_header) + it.offset),
                        it.size / sizeof(uint32_t));

        auto index = (addr - found_key.offset) / fs.sb->sectorsize;
        if (index >= data.size())
            return nt_status::OBJECT_NAME_NOT_FOUND;

        while (index < data.size() && num_pages > 0) {
            {
                lock_guard lg(ino.cache.cache_lock);

                if (first) {
                    src_p = ino.cache.find_page_cache_entry(page_num);
                    first = false;
                } else
                    src_p = ino.cache.next_page_cache_entry(*src_p);
            }

            auto found = ~calc_crc32c(0xffffffff, span((const uint8_t*)get_phys_address(*src_p), fs.sb->sectorsize));

            if (found != data[index]) {
                print("csum at {:x} was {:08x}, expected {:08x}\n", addr, found,
                      data[index]);
                return nt_status::DISK_CORRUPT_ERROR;
            }

            addr += fs.sb->sectorsize;
            page_num++;
            num_pages--;
            index++;
        }

        if (num_pages == 0)
            break;

        // FIXME - need next_leaf function rather than doing lookup again
    } while (true);

    return nt_status::SUCCESS;
}

size_t btrfs_inode::read_extent_into_cache(uint64_t page_num, uint64_t num_pages,
                                           const inode_extent& ie) {
    if (ie.ed.compression != 0) {
        print("FIXME - compression\n"); // FIXME
        return 0;
    }

    const auto& ed2 = *(btrfs_file_extent_item2*)ie.ed.data;

    uint64_t addr = ed2.disk_bytenr + ed2.offset + (page_num << PAGE_SHIFT) - ie.offset;

    num_pages = min(num_pages, ed2.num_bytes >> PAGE_SHIFT);

    auto pages_read = fs.cache.read_into_cache_and_pin(addr >> PAGE_SHIFT,
                                                       num_pages);
    if (pages_read == 0)
        return 0;

    // copy from buffer into our own page cache

    // FIXME - can we steal pages?

    page_cache_entry* dest_p;
    size_t ret = pages_read;

    for (uint64_t i = 0; i < pages_read; i++) {
        page_cache_entry* src_p;

        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = cache.next_page_cache_entry(*dest_p);

            if (dest_p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                dest_p->pfn = allocate_folio(0) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
                if (!dest_p->pfn) {
                    print("allocate_folio failed\n");
                    ret = i;
                    break;
                }
            }
        }

        // FIXME - find next entry rather than doing complete lookup(?)
        {
            lock_guard lg(fs.cache.cache_lock);
            src_p = fs.cache.find_page_cache_entry((addr >> PAGE_SHIFT) + i);
        }

        auto src_va = get_phys_address(*src_p);
        auto dest_va = get_phys_address(*dest_p);

        memcpy(dest_va, src_va, PAGE_SIZE);
    }

    fs.cache.unpin(addr >> PAGE_SHIFT, pages_read);

    if (auto status = check_csums(*this, page_num, addr, num_pages); !success(status)) {
        print("check_csums returned {}\n", status);
        return 0;
    }

    // FIXME - read from another copy (and replace) if csum is bad

    return ret;
}

size_t btrfs_inode::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    uint64_t lastoff = page_num << PAGE_SHIFT;

    // FIXME - extents should be an RB tree

    auto le = extents.next;
    while (le != &extents) {
        auto& ie = containing_record<inode_extent>(le);

        if (ie.offset + ie.ed.ram_bytes < page_num << PAGE_SHIFT) {
            le = le->next;
            continue;
        }

        if (ie.offset > lastoff) {
            auto pages_to_zero = min(num_pages, (ie.offset - lastoff) >> PAGE_SHIFT);
            return read_zeroes_into_cache(page_num, pages_to_zero);
        }

        switch (ie.ed.type) {
            case btrfs_extent_type::inline_extent:
                return read_inline_extent_into_cache(page_num, num_pages, ie);

            case btrfs_extent_type::regular:
                return read_extent_into_cache(page_num, num_pages, ie);

            case btrfs_extent_type::prealloc: {
                auto pages_to_zero = min(num_pages, ie.ed.ram_bytes >> PAGE_SHIFT);
                return read_zeroes_into_cache(page_num, pages_to_zero);
            }

            default:
                print("unknown btrfs extent type {:x}\n", (unsigned int)ie.ed.type);
                return 0;
        }
    }

    return read_zeroes_into_cache(page_num, num_pages);
}

size_t btrfs_inode::flush_cache(uint64_t page_num, uint64_t num_pages) {
    print("FIXME - btrfs_inode::flush_cache ({:x}, {:x})\n", page_num, num_pages); // FIXME

    // FIXME - if more than 128MB, split up

    // FIXME - allocate new chunk if we need to

    // FIXME - punch hole in file extent data
    // FIXME - reduce refcount of extent in extent tree
    // FIXME - if now 0, add back to FST
    // FIXME - if now 0, update BGI
    // FIXME - if now 0, update superblock

    // FIXME - find space for new extent
    // FIXME - carve out of FST
    // FIXME - update BGI
    // FIXME - update superblock
    // FIXME - add to extent tree
    // FIXME - add to csum tree
    // FIXME - add new file extent data

    return 0;
}

static int64_t unix_to_win_time(const btrfs_timespec& t) {
    return (int64_t)((t.sec * 10000000) + (t.nsec / 100) + 116444736000000000);
}

static btrfs_timespec win_to_unix_time(int64_t t) {
    btrfs_timespec ret;

    t -= 116444736000000000;

    ret.sec = (uint64_t)(t / 10000000);
    ret.nsec = (uint32_t)((t % 10000000) * 100);

    return ret;
}

static NTSTATUS query_directory_dirent(btrfs_filesystem& fs, root& r, const btrfs_key& k,
                                       string_view sv, enum btrfs_inode_type inode_type,
                                       span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class) {
    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::DirectoryInformation: {
            if (buf.size() < offsetof(FILE_DIRECTORY_INFORMATION, FileName))
                return nt_status::BUFFER_TOO_SMALL;

            auto utf16len = utf8_to_utf16_len(sv);
            auto len = offsetof(FILE_DIRECTORY_INFORMATION, FileName) + (utf16len * sizeof(char16_t));

            if (buf.size() < len)
                return nt_status::BUFFER_TOO_SMALL;

            auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf.data();

            // FIXME - subvolumes

            btrfs_key search_key;
            btrfs_path p(fs.sb->nodesize);

            switch (k.type) {
                case key_type::INODE_ITEM: {
                    search_key = k;

                    auto status = fs.find_item(r, search_key, p, false);
                    if (!success(status))
                        return status;

                    break;
                }

                case key_type::ROOT_ITEM: {
                    auto* r2 = fs.find_root(k.objectid);
                    if (!r2)
                        return nt_status::DISK_CORRUPT_ERROR;

                    search_key = btrfs_key{r2->ri.root_dirid, key_type::INODE_ITEM, 0};

                    auto status = fs.find_item(*r2, search_key, p, false);
                    if (!success(status))
                        return status;

                    break;
                }

                default:
                    return nt_status::DISK_CORRUPT_ERROR;
            }

            auto& th = *(btrfs_header*)p.data[0];
            auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

            if (p.slots[0] >= th.nritems)
                return nt_status::DISK_CORRUPT_ERROR;

            const auto& it = items[p.slots[0]];
            const auto& found_key = it.key;

            if (found_key.objectid != search_key.objectid || found_key.type != search_key.type)
                return nt_status::DISK_CORRUPT_ERROR;

            if (it.size < sizeof(btrfs_inode_item))
                return nt_status::DISK_CORRUPT_ERROR;

            auto& ii = *(btrfs_inode_item*)((const uint8_t*)&th + sizeof(btrfs_header) + it.offset);

            fdi.NextEntryOffset = 0;
            fdi.FileIndex = 0;
            fdi.CreationTime.QuadPart = unix_to_win_time(ii.otime);
            fdi.LastAccessTime.QuadPart = unix_to_win_time(ii.atime);
            fdi.LastWriteTime.QuadPart = unix_to_win_time(ii.mtime);
            fdi.ChangeTime.QuadPart = unix_to_win_time(ii.ctime);
            fdi.EndOfFile.QuadPart = (int64_t)ii.size;
            fdi.AllocationSize.QuadPart = 0; // FIXME
            fdi.FileAttributes = inode_type == btrfs_inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0; // FIXME
            fdi.FileNameLength = (uint32_t)(utf16len * sizeof(char16_t));
            utf8_to_utf16(span(fdi.FileName, utf16len), sv);

            buf = buf.subspan(0, len);

            return nt_status::SUCCESS;
        }

        default:
            print("btrfs_filesystem::query_directory_dirent: unhandled info class {:x}\n",
                  (unsigned int)file_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

NTSTATUS btrfs_inode::query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index) {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    char name[256]; // FIXME - reference page cache directly
    size_t name_len;
    btrfs_key dir_index_key;
    enum btrfs_inode_type inode_type;
    uint64_t found_index;
    bool found = false;

    shared_lock lg(fs.trans_lock);

    do {
        auto k = btrfs_key{inode, key_type::DIR_INDEX, index};
        btrfs_path p(fs.sb->nodesize);

        auto status = fs.find_item(*r, k, p, false);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            break;

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key.objectid != k.objectid || found_key.type != k.type)
            break;

        if (found_key.offset < index)
            break;

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        // FIXME - return actual error if data malformed

        if (data.size() < offsetof(btrfs_dir_item, name))
            break;

        const auto& di = *(btrfs_dir_item*)data.data();

        if (data.size() < offsetof(btrfs_dir_item, name) + di.data_len + di.name_len)
            break;

        if (di.name_len > sizeof(name))
            break;

        memcpy(name, di.name, di.name_len);
        name_len = di.name_len;

        dir_index_key = di.location;
        inode_type = di.type;
        found_index = found_key.offset;

        found = true;
    } while (false);

    if (!found)
        return nt_status::NO_MORE_FILES;

    auto ret = query_directory_dirent(fs, *r, dir_index_key, string_view(name, name_len),
                                      inode_type, buf,
                                      file_information_class);

    index = found_index + 1;

    return ret;
}

static void write_superblocks(btrfs_filesystem& fs) {
    // FIXME - write the superblocks asynchronously and wait (will need multiple copies of superblock in memory, for the different addresses)

    for (auto addr : superblock_addrs) {
        if (addr + sizeof(btrfs_super_block) >= fs.vol->size)
            break;

        auto& sb = *fs.sb;

        sb.bytenr = addr;
        *(uint32_t*)&sb= ~calc_crc32c(0xffffffff, span((const uint8_t*)&sb.fsid, sizeof(btrfs_super_block) - sizeof(sb.csum)));

        auto iter = folio_iter(phys_memory_pfn(fs.sb) << PAGE_SHIFT);

        uint64_t written_pages;

        auto status = fs.vol->dio_write(addr >> PAGE_SHIFT,
                                        bytes_to_pages(sizeof(btrfs_super_block)),
                                        iter, written_pages, true);
        if (!success(status)) // FIXME - go readonly if this happens?
            print("volume::dio_write returned {}\n", status);
    }
}

static nt_status flush_root(btrfs_filesystem& fs, root& r) {
    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(*fs.root_root, {r.id, key_type::ROOT_ITEM, 0}, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] >= th.nritems) {
        print("Root item not found\n");
        return nt_status::DISK_CORRUPT_ERROR;
    }

    const auto& it = items[p.slots[0]];
    const auto& k = it.key;

    if (k.objectid != r.id || k.type != key_type::ROOT_ITEM) {
        print("Root item not found\n");
        return nt_status::DISK_CORRUPT_ERROR;
    }

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (data.size() < sizeof(btrfs_root_item)) {
        print("Root item truncated\n");
        return nt_status::DISK_CORRUPT_ERROR;
    }

    auto& ri = *(btrfs_root_item*)data.data();

    memcpy(&ri, &r.ri, sizeof(btrfs_root_item));

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status adjust_free_space_info(btrfs_filesystem& fs, uint64_t address,
                                        int delta) {
    auto& r = *fs.free_space_root;
    nt_status status;
    btrfs_path p(fs.sb->nodesize);
    chunk* c = find_chunk(fs, address);

    if (!c) {
        print("Could not find chunk for logical address {:x}\n", address);
        return nt_status::INTERNAL_ERROR;
    }

    status = fs.find_item(r, {c->offset, key_type::FREE_SPACE_INFO, 0}, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    auto& it = items[p.slots[0]];
    const auto& found_key = it.key;

    if (found_key.objectid != c->offset || found_key.type != key_type::FREE_SPACE_INFO)
        return nt_status::INTERNAL_ERROR;

    auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

    if (it.size < sizeof(btrfs_free_space_info))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& fsi = *(btrfs_free_space_info*)data.data();

    fsi.extent_count = (uint32_t)((int32_t)fsi.extent_count + delta);

    // copy tree back into page cache and mark dirty

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    return nt_status::SUCCESS;
}

static nt_status remove_free_space(btrfs_filesystem& fs, uint64_t address,
                                   uint64_t length) {
    auto& r = *fs.free_space_root;
    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(r, {address, key_type::FREE_SPACE_EXTENT, 0xffffffffffffffff},
                               p, true);
    if (!success(status))
        return status;

    // FIXME - if bitmap, convert into normal extents

    while (true) {
        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        if (p.slots[0] >= th.nritems)
            return nt_status::OBJECT_NAME_NOT_FOUND;

        const auto& it = items[p.slots[0]];
        const auto& k = it.key;

        if (k.objectid > address) {
            if (!fs.move_path_back(p))
                return nt_status::OBJECT_NAME_NOT_FOUND;
            continue;
        }

        if (k.objectid > address || k.objectid + k.offset < address + length || k.type != key_type::FREE_SPACE_EXTENT)
            return nt_status::OBJECT_NAME_NOT_FOUND;

        break;
    }

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    const auto& it = items[p.slots[0]];
    auto k = it.key;

    if (k.objectid == address && k.offset == length) {
        // removing whole entry
        status = fs.remove_item2(p);
        if (!success(status))
            return status;

        status = adjust_free_space_info(fs, address, -1);
        if (!success(status))
            return status;
    } else if (k.objectid == address) {
        // removing start
        status = fs.change_key(p, {k.objectid + length, key_type::FREE_SPACE_EXTENT, k.offset - length});
        if (!success(status))
            return status;
    } else if (k.objectid + k.offset == address + length) {
        // removing end
        status = fs.change_key(p, {k.objectid, key_type::FREE_SPACE_EXTENT, k.offset - length});
        if (!success(status))
            return status;
    } else {
        // removing middle

        status = fs.change_key(p, {k.objectid, key_type::FREE_SPACE_EXTENT, address - k.objectid});
        if (!success(status))
            return status;

        status = insert_item(fs, r, {address + length, key_type::FREE_SPACE_EXTENT, k.objectid + k.offset - address - length},
                             span<const uint8_t>());
        if (!success(status))
            return status;

        status = adjust_free_space_info(fs, address, 1);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static nt_status add_free_space(btrfs_filesystem& fs, uint64_t address,
                                uint64_t length) {
    auto& r = *fs.free_space_root;
    btrfs_path p(fs.sb->nodesize);

    auto status = fs.find_item(r, {address, key_type::FREE_SPACE_EXTENT, 0xffffffffffffffff},
                               p, true);
    if (!success(status))
        return status;

    // FIXME - if bitmap, convert into normal extents

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    btrfs_key next_k;
    btrfs_key prev_k{0, (enum key_type)0, 0};

    if (p.slots[0] < th.nritems)
        next_k = items[p.slots[0]].key;
    else
        next_k = {0, (enum key_type)0, 0};

    if (fs.move_path_back(p)) {
        auto& th2 = *(btrfs_header*)p.data[0];
        auto items2 = span((btrfs_item*)((uint8_t*)&th2 + sizeof(btrfs_header)), th2.nritems);

        if (items2[p.slots[0]].key.type == key_type::FREE_SPACE_EXTENT)
            prev_k = items2[p.slots[0]].key;
    }

    if (prev_k.type == key_type::FREE_SPACE_EXTENT &&
        next_k.type == key_type::FREE_SPACE_EXTENT &&
        prev_k.objectid + prev_k.offset == address &&
        next_k.objectid == address + length) {
        // bridge

        status = fs.change_key(p, {prev_k.objectid, key_type::FREE_SPACE_EXTENT, prev_k.offset + length + next_k.offset});
        if (!success(status))
            return status;

        status = fs.remove_item(r, next_k);
        if (!success(status))
            return status;

        status = adjust_free_space_info(fs, address, -1);
        if (!success(status))
            return status;

        return nt_status::SUCCESS;
    }

    if (prev_k.type == key_type::FREE_SPACE_EXTENT &&
        prev_k.objectid + prev_k.offset == address) {
        // extend previous
        return fs.change_key(p, {prev_k.objectid, key_type::FREE_SPACE_EXTENT, prev_k.offset + length});
    }

    if (next_k.type == key_type::FREE_SPACE_EXTENT &&
        next_k.objectid == address + length) {
        // extend next

        if (!fs.move_path_forward(p))
            return nt_status::INTERNAL_ERROR;

        return fs.change_key(p, {address, key_type::FREE_SPACE_EXTENT, next_k.offset + length});
    }

    // new entry

    status = insert_item(fs, r, {address, key_type::FREE_SPACE_EXTENT, length},
                         span<const uint8_t>());
    if (!success(status))
        return status;

    status = adjust_free_space_info(fs, address, 1);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

static nt_status adjust_chunk_usage(btrfs_filesystem& fs, chunk& c, int64_t delta) {
    btrfs_path p(fs.sb->nodesize);
    btrfs_key search_key = {c.offset, key_type::BLOCK_GROUP_ITEM, c.ci.length};

    auto status = fs.find_item(*fs.block_group_root, search_key, p, true);
    if (!success(status))
        return status;

    auto& th = *(btrfs_header*)p.data[0];
    auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

    if (p.slots[0] >= th.nritems)
        return nt_status::DISK_CORRUPT_ERROR;

    const auto& it = items[p.slots[0]];
    const auto& k = it.key;

    if (k != search_key || it.size < sizeof(btrfs_block_group_item))
        return nt_status::DISK_CORRUPT_ERROR;

    auto& bgi = *(btrfs_block_group_item*)((uint8_t*)&th + sizeof(btrfs_header) + it.offset);

    bgi.used = (uint64_t)((int64_t)bgi.used + delta);

    if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    fs.sb->bytes_used = (uint64_t)((int64_t)fs.sb->bytes_used + delta);

    return nt_status::SUCCESS;
}

static nt_status do_metadata_changes(btrfs_filesystem& fs) {
    list_entry changed_chunks = { &changed_chunks, &changed_chunks };

    while (!is_list_empty(fs.metadata_changes)) {
        auto& mc = containing_record<metadata_change>(fs.metadata_changes.next);

        auto c = find_chunk(fs, mc.address);
        if (!c) {
            print("no chunk found for address {:x}\n", mc.address);
            return nt_status::INTERNAL_ERROR;
        }

        if (!c->changed) {
            insert_tail_list(changed_chunks, c->changed_le);
            c->changed = true;
            c->delta = 0;
        }

        if (mc.remove) {
            // FIXME - snapshotted metadata

            auto status = fs.remove_item(*fs.extent_root, {mc.address, key_type::METADATA_ITEM, mc.level});
            if (!success(status)) {
                print("remove_item returned {}\n", status);
                return status;
            }

            status = add_free_space(fs, mc.address, fs.sb->nodesize);
            if (!success(status)) {
                print("add_free_space returned {}\n", status);
                return status;
            }

            c->delta -= fs.sb->nodesize;

            auto sc = new chunk_space;
            if (!sc)
                return nt_status::INSUFFICIENT_RESOURCES;

            if (is_list_empty(c->pending_space_list))
                insert_tail_list(fs.space_pending_chunks, c->space_pending_le);

            sc->offset = mc.address;
            sc->length = fs.sb->nodesize;
            insert_tail_list(c->pending_space_list, sc->le);
        } else {
            extent_item_tbr eitbr;

            eitbr.ei.refs = 1;
            eitbr.ei.generation = fs.sb->generation;
            eitbr.ei.flags = BTRFS_EXTENT_FLAG_TREE_BLOCK;
            eitbr.type = key_type::TREE_BLOCK_REF;
            eitbr.tbr.offset = mc.root;

            auto status = insert_item(fs, *fs.extent_root, {mc.address, key_type::METADATA_ITEM, mc.level},
                                      span((uint8_t*)&eitbr, sizeof(eitbr)));
            if (!success(status)) {
                print("insert_item returned {}\n", status);
                return status;
            }

            status = remove_free_space(fs, mc.address, fs.sb->nodesize);
            if (!success(status)) {
                print("remove_free_space returned {}\n", status);
                return status;
            }

            c->delta += fs.sb->nodesize;
        }

        remove_entry_list(mc.le);
        free(&mc);
    }

    while (!is_list_empty(changed_chunks)) {
        auto& c = *(chunk*)((uint8_t*)changed_chunks.next - offsetof(chunk, changed_le));

        c.changed = false;
        remove_entry_list(c.changed_le);

        if (c.delta == 0)
            continue;

        if (auto status = adjust_chunk_usage(fs, c, c.delta); !success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static void update_superblock_backup(btrfs_filesystem& fs) {
    memmove(&fs.sb->super_roots[1], &fs.sb->super_roots[0],
            sizeof(btrfs_root_backup) * (fs.sb->super_roots.size() - 1));

    fs.sb->super_roots[0].tree_root = fs.sb->root;
    fs.sb->super_roots[0].tree_root_gen = fs.sb->generation;
    fs.sb->super_roots[0].tree_root_level = fs.sb->root_level;
    fs.sb->super_roots[0].chunk_root = fs.sb->chunk_root;
    fs.sb->super_roots[0].chunk_root_gen = fs.sb->chunk_root_generation;
    fs.sb->super_roots[0].chunk_root_level = fs.sb->chunk_root_level;
    fs.sb->super_roots[0].extent_root = fs.extent_root->ri.bytenr;
    fs.sb->super_roots[0].extent_root_gen = fs.extent_root->ri.generation;
    fs.sb->super_roots[0].extent_root_level = fs.extent_root->ri.level;
    fs.sb->super_roots[0].fs_root = fs.fs_root->ri.bytenr;
    fs.sb->super_roots[0].fs_root_gen = fs.fs_root->ri.generation;
    fs.sb->super_roots[0].fs_root_level = fs.fs_root->ri.level;
    fs.sb->super_roots[0].dev_root = fs.dev_root->ri.bytenr;
    fs.sb->super_roots[0].dev_root_gen = fs.dev_root->ri.generation;
    fs.sb->super_roots[0].dev_root_level = fs.dev_root->ri.level;
    fs.sb->super_roots[0].csum_root = fs.csum_root->ri.bytenr;
    fs.sb->super_roots[0].csum_root_gen = fs.csum_root->ri.generation;
    fs.sb->super_roots[0].csum_root_level = fs.csum_root->ri.level;
    fs.sb->super_roots[0].total_bytes = fs.sb->total_bytes;
    fs.sb->super_roots[0].bytes_used = fs.sb->bytes_used;
    fs.sb->super_roots[0].num_devices = fs.sb->num_devices;
}

nt_status btrfs_filesystem::flush(bool make_readonly) {
    lock_guard lock(trans_lock);

    nt_status status;

    // FIXME - should have rwsem held exclusively

    if (is_list_empty(dirty_root_list) || readonly) { // no changes made
        if (make_readonly)
            go_readonly();

        return nt_status::SUCCESS;
    }

    update_superblock_backup(*this);

    status = do_metadata_changes(*this);
    if (!success(status)) {
        print("do_metadata_changes returned {}, going readonly\n", status);
        go_readonly();
        return status;
    }

    while (!is_list_empty(dirty_root_list)) {
        auto& r = *(root*)((uint8_t*)dirty_root_list.next - offsetof(root, dirty_le));

        auto status = flush_root(*this, r);
        if (!success(status)) {
            print("flush_root returned {}, going readonly\n", status);
            go_readonly();
            return status;
        }

        remove_entry_list(r.dirty_le);
        r.is_dirty = false;
    }

    status = do_metadata_changes(*this);
    if (!success(status)) {
        print("do_metadata_changes returned {}, going readonly\n", status);
        go_readonly();
        return status;
    }

    // loop through cache, and call flush_cache for dirty ranges

    while (true) {
        page_cache_entry* start;
        uint64_t num_pages;

        if (!cache.find_dirty_range(start, num_pages))
            break;

        auto first_page = start->tree.key;

        while (true) {
            auto flushed = flush_cache(first_page, num_pages);

            if (flushed == 0) {
                print("flush_cache did not flush any pages, going readonly\n");
                go_readonly();
                return nt_status::INTERNAL_ERROR;
            }

            if (num_pages >= flushed)
                break;

            first_page += flushed;
            num_pages -= flushed;
        }
    }

    status = vol->flush();
    if (!success(status)) {
        print("volume::flush returned {}, going readonly\n", status);
        go_readonly();
        return status;
    }

    sb->root_level = root_root->ri.level;
    sb->root = root_root->ri.bytenr;

    write_superblocks(*this);

    // FIXME - send SATA command again to ensure cache flushed

    sb->generation++;

    while (!is_list_empty(space_pending_chunks)) {
        auto& c = *(chunk*)((uint8_t*)space_pending_chunks.next - offsetof(chunk, space_pending_le));

        handle_pending_space(c);

        remove_entry_list(c.space_pending_le);
    }

    if (make_readonly)
        go_readonly();

    return nt_status::SUCCESS;
}

void btrfs_filesystem::flush_thread() {
    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("create_timer failed, exiting flush thread\n");
        return;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    int64_t due_time_ticks = 300000000; // 30 seconds
    auto due_time_ms = (int32_t)((due_time_ticks * 1000) / SYSTEM_TIME_RESOLUTION);

    auto status = tim.set(-due_time_ticks, nullptr, nullptr, false,
                          due_time_ms, nullptr);
    if (!success(status)) {
        print("timer::set returned {}, exiting flush thread\n", status);
        return;
    }

    while (true) {
        status = wait_for_object(tim, true);

        auto status2 = flush(false);
        if (!success(status2))
            break;

        if (status != nt_status::SUCCESS) // thread has been killed
            break;
    }
}

void btrfs_filesystem::go_readonly() {
    if (readonly)
        return;

    readonly = true;

    if (flush_thread_obj) {
        auto& t = static_cast<::thread&>(flush_thread_obj.get());

        t.send_signal((uint64_t)nt_status::MEDIA_WRITE_PROTECTED);
    }
}

chunk::~chunk() {
    while (!is_list_empty(space_list)) {
        auto& s = containing_record<chunk_space>(space_list.next);

        remove_entry_list(s.le);
        free(&s);
    }
}

NTSTATUS btrfs_inode::set_information(span<const uint8_t> buffer,
                                      FILE_INFORMATION_CLASS file_information_class) {
    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::BasicInformation:
            return set_basic_information(buffer);

        default:
            print("btrfs_inode::set_information: unhandled file information class {}\n",
                  (unsigned int)file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status btrfs_inode::set_basic_information(span<const uint8_t> buffer) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    const auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    if (fbi.CreationTime.QuadPart != 0 || fbi.LastAccessTime.QuadPart != 0 ||
        fbi.LastWriteTime.QuadPart != 0 || fbi.ChangeTime.QuadPart != 0) {

        btrfs_path p(fs.sb->nodesize);
        nt_status status;

        lock_guard lock(fs.trans_lock);

        status = fs.find_item(*r, {inode, key_type::INODE_ITEM, 0}, p, true);
        if (!success(status))
            return status;

        auto& th = *(btrfs_header*)p.data[0];
        auto items = span((btrfs_item*)((uint8_t*)&th + sizeof(btrfs_header)), th.nritems);

        auto& it = items[p.slots[0]];
        const auto& found_key = it.key;

        if (found_key.objectid != inode || found_key.type != key_type::INODE_ITEM)
            return nt_status::DISK_CORRUPT_ERROR;

        auto data = span((const uint8_t*)&th + sizeof(btrfs_header) + it.offset, it.size);

        if (it.size < sizeof(btrfs_inode_item))
            return nt_status::DISK_CORRUPT_ERROR;

        auto& ii = *(btrfs_inode_item*)data.data();

        ii.sequence++;

        // FIXME - setting times to -1 or -2
        // FIXME - subsequent writes etc. shouldn't clobber set time

        if (fbi.CreationTime.QuadPart != 0)
            ii.otime = win_to_unix_time(fbi.CreationTime.QuadPart);

        if (fbi.LastAccessTime.QuadPart != 0)
            ii.atime = win_to_unix_time(fbi.LastAccessTime.QuadPart);

        if (fbi.LastWriteTime.QuadPart != 0)
            ii.mtime = win_to_unix_time(fbi.LastWriteTime.QuadPart);

        if (fbi.ChangeTime.QuadPart != 0)
            ii.ctime = win_to_unix_time(fbi.ChangeTime.QuadPart);

        // copy tree back into page cache and mark dirty

        if (!fs.cache.write(span(p.data[0], fs.sb->nodesize), th.bytenr)) // FIXME - return status
            return nt_status::INTERNAL_ERROR;
    }

    // FIXME - FileAttributes

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::query_standard_information(span<uint8_t> buffer, uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STANDARD_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsi = *(FILE_STANDARD_INFORMATION*)buffer.data();

    fsi.AllocationSize.QuadPart = (int64_t)ii.nbytes;
    fsi.EndOfFile.QuadPart = (int64_t)ii.size;
    fsi.NumberOfLinks = ii.nlink;
    fsi.DeletePending = 0; // FIXME
    fsi.Directory = inode_type == inode_type::directory ? 1 : 0;

    retlen = sizeof(fsi);

    return nt_status::SUCCESS;
}

NTSTATUS btrfs_inode::query_information(span<uint8_t> buffer,
                                        FILE_INFORMATION_CLASS file_information_class,
                                        uint64_t& retlen) {
    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::StandardInformation:
            return query_standard_information(buffer, retlen);

        default:
            print("btrfs_inode::query_information: unhandled file information class {}\n",
                 (unsigned int)file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}
