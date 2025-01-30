#include "kernel.h"
#include "objects.h"
#include "btrfs.h"
#include "handle.h"
#include "crc32c.h"

using namespace std;

static const uint64_t superblock_addrs[] = { 0x10000, 0x4000000, 0x4000000000, 0x4000000000000, 0 };

static const uint64_t supported_incompat_flags = BTRFS_INCOMPAT_FLAGS_MIXED_BACKREF | BTRFS_INCOMPAT_FLAGS_MIXED_GROUPS |
                                                 BTRFS_INCOMPAT_FLAGS_BIG_METADATA | BTRFS_INCOMPAT_FLAGS_EXTENDED_IREF |
                                                 BTRFS_INCOMPAT_FLAGS_SKINNY_METADATA | BTRFS_INCOMPAT_FLAGS_NO_HOLES;

btrfs_filesystem::btrfs_filesystem(volume& vol) : filesystem(vol) {
    vol.acquire();

    uint64_t read_bytes;
    auto status = vol.read(span((uint8_t*)&sb, sizeof(superblock)), superblock_addrs[0],
                           read_bytes);

    if (!success(status)) {
        printk("volume::read returned ");
        print_hex((uint64_t)status, 8);
        printk("\n");
        return;
    } else if (read_bytes < sizeof(superblock)) {
        printk("volume::read failed\n");
        return;
    }

    if (sb.magic != btrfs_magic)
        return;

    // FIXME - check checksum
    // FIXME - check other superblock addresses, and choose one with highest generation

    if (sb.incompat_flags & ~supported_incompat_flags) {
        printk("Unsupported incompat flags\n");
        return;
    }

    if (sb.num_devices != 1) {
        printk("Multiple devices not supported\n");
        return;
    }

    if (sb.node_size & (PAGE_SIZE - 1)) {
        printk("btrfs node size not multiple of CPU page size\n");
        return;
    }

    if (sb.sector_size & (PAGE_SIZE - 1)) {
        printk("btrfs sector size not multiple of CPU page size\n");
        return;
    }

    // FIXME - make sure sb.n < sizeof(sb.sys_chunk_array)
    auto chunk_array = span(sb.sys_chunk_array.data(), sb.n);

    while (!chunk_array.empty()) {
        if (chunk_array.size() < sizeof(key)) {
            printk("Chunk array truncated\n");
            return;
        }

        auto& k = *(key*)chunk_array.data();

        if (k.obj_type != key_type::CHUNK_ITEM) {
            printk("Unexpected item in chunk array\n");
            return;
        }

        chunk_array = chunk_array.subspan(sizeof(key));

        if (chunk_array.size() < offsetof(chunk_item, stripes)) {
            printk("Chunk array truncated\n");
            return;
        }

        auto& ci = *(chunk_item*)chunk_array.data();
        size_t chunk_size = offsetof(chunk_item, stripes) + (sizeof(chunk_item_stripe) * ci.num_stripes);

        if (chunk_array.size() < chunk_size) {
            printk("Chunk array truncated\n");
            return;
        }

        if (ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
            printk("RAID not supported\n");
            return;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            printk("malloc failed\n");
            return;
        }

        auto& c = *cptr;

        c.offset = k.offset;
        memcpy(&c.ci, chunk_array.data(), chunk_size);
        insert_tail_list(chunk_list, c.le);

        chunk_array = chunk_array.subspan(chunk_size);
    }

    {
        auto rptr = new root;
        if (!rptr) {
            printk("new failed\n");
            return;
        }

        auto& r = *rptr;
        r.id = BTRFS_ROOT_ROOT;
        r.ri.block_number = sb.root_tree_addr;
        r.ri.root_level = sb.root_level;

        insert_tail_list(root_list, r.le);
    }

    {
        auto rptr = new root;
        if (!rptr) {
            printk("new failed\n");
            return;
        }

        auto& r = *rptr;
        r.id = BTRFS_ROOT_CHUNK;
        r.ri.block_number = sb.chunk_tree_addr;
        r.ri.root_level = sb.chunk_root_level;

        insert_tail_list(root_list, r.le);
    }

    if (!load_chunks()) {
        printk("load_chunks failed\n");
        return;
    }

    if (!load_roots()) {
        printk("load_roots failed\n");
        return;
    }

    // FIXME - default subvol

    root_inode = new btrfs_inode(*this, BTRFS_ROOT_FSTREE, SUBVOL_ROOT_INODE, btrfs_inode_type::directory);
    if (!root_inode) {
        printk("new failed\n");
        return;
    }

    if (!root_inode->valid)
        return;

    insert_tail_list(inode_list, root_inode->le);

    valid = true;
}

bool btrfs_probe(volume& vol) {
    auto fs = new btrfs_filesystem(vol);
    if (!fs) {
        printk("new failed\n");
        return false;
    }

    if (!fs->valid) {
        delete fs;
        return false;
    }

    add_fs_to_list(*fs);

    {
        lock_guard lg(vol.mount_lock);
        vol.filesystem = *fs;
    }

    return true;
}

bool btrfs_filesystem::read_metadata(uint64_t addr, span<uint8_t> data) {
    if (!cache.read(data, addr)) {
        printk("Short read\n");
        return false;
    }

    // FIXME - check checksum, address, level, and subvol ID
    // FIXME - if duff, read from other version (and replace)

    return true;
}

bool btrfs_filesystem::walk_tree2(uint64_t addr, uint8_t exp_level, invocable<const key&, span<const uint8_t>> auto func) {
    auto bufptr = (uint8_t*)malloc(sb.node_size);
    if (!bufptr) {
        printk("malloc failed\n");
        return false;
    }

    auto buf = span(bufptr, sb.node_size);

    if (!read_metadata(addr, buf)) {
        printk("read_metadata failed\n");
        free(bufptr);
        return false;
    }

    auto& th = *(tree_header*)buf.data();

    if (th.level != exp_level) {
        char s[100];
        auto sp = span(s, sizeof(s));

        sp = sp_strcpy(sp, "Tree level was incorrect (");
        sp = sp_itoa_hex(sp, th.level);
        sp = sp_strcpy(sp, ", expected ");
        sp = sp_itoa_hex(sp, exp_level);
        sp = sp_strcpy(sp, ")\n");
        printk(string_view(s, sp.data() - s));

        free(bufptr);
        return false;
    }

    if (th.level == 0) {
        auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        for (const auto& i : items) {
            func(i.k, span<const uint8_t>((uint8_t*)&th + sizeof(tree_header) + i.offset, i.size));
        }
    } else {
        auto items = span((internal_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        for (const auto& i : items) {
            if (!walk_tree2(i.address, exp_level - 1, func))
                break;
        }
    }

    free(bufptr);

    return true;
}

bool btrfs_filesystem::walk_tree(uint64_t id, invocable<const key&, span<const uint8_t>> auto func) {
    uint64_t addr = 0;
    uint8_t level;

    auto le = root_list.next;
    while (le != &root_list) {
        auto& r = containing_record<root>(le);

        if (r.id == id) {
            addr = r.ri.block_number;
            level = r.ri.root_level;
            break;
        }

        le = le->next;
    }

    if (addr == 0) {
        printk("Could not find address for root ");
        print_hex(id);
        printk("\n");
        return false;
    }

    return walk_tree2(addr, level, func);
}

bool btrfs_filesystem::load_chunks() {
    list_entry new_chunk_list = { &new_chunk_list, &new_chunk_list };

    auto b = walk_tree(BTRFS_ROOT_CHUNK, [&](const key& k, span<const uint8_t> data) {
        if (k.obj_type != key_type::CHUNK_ITEM)
            return;

        if (data.size() < offsetof(chunk_item, stripes)) {
            printk("Chunk item truncated\n");
            return;
        }

        auto& ci = *(chunk_item*)data.data();
        size_t chunk_size = offsetof(chunk_item, stripes) + (sizeof(chunk_item_stripe) * ci.num_stripes);

        if (data.size() < chunk_size) {
            printk("Chunk array truncated\n");
            return;
        }

        if (ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
            printk("RAID not supported\n");
            return;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            printk("malloc failed\n");
            return;
        }

        auto& c = *cptr;

        c.offset = k.offset;
        memcpy(&c.ci, data.data(), chunk_size);
        insert_tail_list(new_chunk_list, c.le);
    });

    if (!b) {
        swap_lists(chunk_list, new_chunk_list);
        free_chunk_list();
        swap_lists(chunk_list, new_chunk_list);
        return false;
    }

    free_chunk_list();
    swap_lists(chunk_list, new_chunk_list);

    return true;
}

uint64_t btrfs_filesystem::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    uint64_t addr = page_num << PAGE_SHIFT;

    // find chunk

    optional<reference_wrapper<chunk>> crw;

    {
        auto le = chunk_list.next;
        while (le != &chunk_list) {
            auto& c = containing_record<chunk>(le);

            if (addr >= c.offset && addr < c.offset + c.ci.size) {
                crw = c;
                break;
            }

            le = le->next;
        }
    }

    if (!crw) {
        printk("Could not find chunk for logical address ");
        print_hex(addr);
        printk("\n");
        return 0;
    }

    auto& c = crw->get();

    if (c.ci.type & BLOCK_FLAG_RAID0) {
        printk("RAID0 not supported\n");
        return 0;
    } else if (c.ci.type & BLOCK_FLAG_RAID10) {
        printk("RAID10 not supported\n");
        return 0;
    } else if (c.ci.type & BLOCK_FLAG_RAID5) {
        printk("RAID10 not supported\n");
        return 0;
    } else if (c.ci.type & BLOCK_FLAG_RAID6) {
        printk("RAID10 not supported\n");
        return 0;
    }

    // chunk is SINGLE, DUP, RAID1, RAID1C3, or RAID1C4

    // FIXME - choose stripe and device
    // FIXME - handle missing devices

    uint64_t disk_addr = (addr - c.offset + c.ci.stripes[0].offset);

    auto pages_read = vol.read_into_cache_and_pin(disk_addr >> PAGE_SHIFT,
                                                  num_pages);
    if (pages_read == 0)
        return 0;

    // copy from buffer into our own page cache

    // FIXME - can we steal pages?

    page_cache_entry* dest_p;

    for (uint64_t i = 0; i < pages_read; i++) {
        page_cache_entry* src_p;

        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = ::find_page_cache_entry(cache.cache, page_num);
            else
                dest_p = &containing_record<page_cache_entry>(dest_p->le.next);
        }

        src_p = vol.find_page_cache_entry((disk_addr >> PAGE_SHIFT) + i);
        // FIXME - find next entry rather than doing complete lookup

        auto src_va = phys_address(src_p->phys_address);
        auto dest_va = phys_address(dest_p->phys_address);

        memcpy(dest_va, src_va, PAGE_SIZE);
    }

    vol.unpin(disk_addr >> PAGE_SHIFT, pages_read);

    // FIXME - check csum (metadata or data)
    // FIXME - choose other copy if csum wrong
    // FIXME - copy over bad copy if csum wrong

    return pages_read;
}

uint64_t btrfs_filesystem::read_into_cache_and_pin(uint64_t page_num,
                                                   uint64_t num_pages) {
    if (!cache.pin(page_num, num_pages, nullptr))
        return 0;

    auto read_pages = read_into_cache(page_num, num_pages);

    if (read_pages < num_pages)
        cache.unpin(page_num + read_pages, num_pages - read_pages);

    return read_pages;
}

void btrfs_filesystem::unpin(uint64_t page_num, uint64_t num_pages) {
    cache.unpin(page_num, num_pages);
}

page_cache_entry* btrfs_filesystem::find_page_cache_entry(uint64_t page_num) {
    return ::find_page_cache_entry(cache.cache, page_num);
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

bool btrfs_filesystem::load_roots() {
    list_entry new_root_list = { &new_root_list, &new_root_list };

    auto b = walk_tree(BTRFS_ROOT_ROOT, [&](const key& k, span<const uint8_t> data) {
        if (k.obj_type != key_type::ROOT_ITEM)
            return;

        if (data.size() < sizeof(root_item)) {
            printk("Root item truncated\n");
            return;
        }

        auto rptr = new root;
        if (!rptr) {
            printk("new failed\n");
            return;
        }

        auto& r = *rptr;

        r.id = k.obj_id;
        memcpy(&r.ri, data.data(), sizeof(root_item));
        insert_tail_list(new_root_list, r.le);
    });

    if (!b) {
        swap_lists(root_list, new_root_list);
        free_root_list();
        swap_lists(root_list, new_root_list);
        return false;
    }

    free_root_list();
    swap_lists(root_list, new_root_list);

    return true;
}

bool btrfs_filesystem::walk_from2(uint64_t addr, uint8_t exp_level, const key& k,
                                  invocable<const key&, span<const uint8_t>> auto func) {
    bool ret = false;

    auto bufptr = (uint8_t*)malloc(sb.node_size);
    if (!bufptr) {
        printk("malloc failed\n");
        return 0;
    }

    auto buf = span(bufptr, sb.node_size);

    if (!read_metadata(addr, buf)) {
        printk("read_metadata failed\n");
        free(bufptr);
        return 0;
    }

    auto& th = *(tree_header*)buf.data();

    if (th.level != exp_level) {
        char s[100];
        auto sp = span(s, sizeof(s));

        sp = sp_strcpy(sp, "Tree level was incorrect (");
        sp = sp_itoa_hex(sp, th.level);
        sp = sp_strcpy(sp, ", expected ");
        sp = sp_itoa_hex(sp, exp_level);
        sp = sp_strcpy(sp, ")\n");
        printk(string_view(s, sp.data() - s));

        free(bufptr);
        return 0;
    }

    if (th.level == 0) {
        auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        for (const auto& i : items) {
            if (i.k < k)
                continue;

            ret = func(i.k, span<const uint8_t>((uint8_t*)&th + sizeof(tree_header) + i.offset, i.size));

            if (!ret)
                break;
        }
    } else {
        auto items = span((internal_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        for (unsigned int i = 0; i < items.size(); i++) {
            if (i != items.size() - 1 && items[i+1].k < k)
                continue;

            ret = walk_from2(items[i].address, exp_level - 1, k, func);

            if (!ret)
                break;
        }
    }

    free(bufptr);

    return ret;
}

void btrfs_filesystem::walk_from(uint64_t subvol, const key& k, invocable<const key&, span<const uint8_t>> auto func) {
    uint64_t addr = 0;
    uint8_t level;

    auto le = root_list.next;
    while (le != &root_list) {
        auto& r = containing_record<root>(le);

        if (r.id == subvol) {
            addr = r.ri.block_number;
            level = r.ri.root_level;
            break;
        }

        le = le->next;
    }

    if (addr == 0) {
        printk("Could not find address for root ");
        print_hex(subvol);
        printk("\n");
        return;
    }

    walk_from2(addr, level, k, func);
}

btrfs_inode::btrfs_inode(btrfs_filesystem& fs, uint64_t subvol, uint64_t inode, enum btrfs_inode_type inode_type) : fs(fs), subvol(subvol), inode(inode) {
    bool found_inode_item = false;

    fs.walk_from(subvol, key(inode, key_type::INODE_ITEM, 0), [&](const key& k, span<const uint8_t> data) {
        if (k.obj_id != inode)
            return false;

        switch (k.obj_type) {
            case key_type::INODE_ITEM:
                if (data.size() < sizeof(inode_item)) {
                    printk("INODE_ITEM was short\n");
                    return false;
                }

                memcpy(&ii, data.data(), min(data.size(), sizeof(inode_item)));
                found_inode_item = true;
            break;

            case key_type::DIR_INDEX: {
                auto& di = *(dir_item*)data.data();

                if (data.size() < offsetof(dir_item, name)) {
                    printk("DIR_ITEM was short\n");
                    return false;
                }

                if (data.size() < offsetof(dir_item, name) + di.m + di.n) {
                    printk("DIR_ITEM was short\n");
                    return false;
                }

                if (di.n > 255) {
                    printk("Skipping item with overly long name\n");
                    return true;
                }

                auto deptr = (dirent*)malloc(offsetof(dirent, name) + di.n);
                if (!deptr) {
                    printk("malloc failed\n");
                    return false;
                }

                auto& de = *deptr;

                de.inode = di.k.obj_id;
                de.type = di.k.obj_type;
                de.inode_type = di.type;
                de.index = k.offset;
                de.namelen = (uint8_t)di.n;
                memcpy(de.name, di.name, di.n);

                {
                    lock_guard lock(children_lock);
                    insert_tail_list(children, de.le);
                }

                break;
            }

            case key_type::EXTENT_DATA: {
                auto& ed = *(extent_data*)data.data();

                if (data.size() < offsetof(extent_data, data)) {
                    printk("EXTENT_DATA was short\n");
                    return false;
                }

                if (ed.compression != 0) {
                    printk("Compression not supported\n");
                    return false;
                }

                if (ed.encryption != 0) {
                    printk("Encryption not supported\n");
                    return false;
                }

                if (ed.encoding != 0) {
                    printk("Other encodings not supported\n");
                    return false;
                }

                if (ed.type == btrfs_extent_type::regular || ed.type == btrfs_extent_type::prealloc) {
                    if (data.size() < offsetof(extent_data, data) + sizeof(extent_data2)) {
                        printk("EXTENT_DATA was short\n");
                        return false;
                    }
                } else if (ed.type == btrfs_extent_type::inline_extent) {
                    if (data.size() < offsetof(extent_data, data) + ed.decoded_size) {
                        printk("EXTENT_DATA was short\n");
                        return false;
                    }
                } else {
                    printk("Invalid extent type\n");
                    return false;
                }

                if (ed.type == btrfs_extent_type::regular) {
                    auto& ed2 = *(extent_data2*)ed.data;

                    if (ed2.address == 0 || ed2.size == 0) // skip sparse extents
                        return true;
                }

                auto ieptr = (inode_extent*)malloc(offsetof(inode_extent, ed) + data.size());
                if (!ieptr) {
                    printk("malloc failed\n");
                    return false;
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

        // FIXME - record other entries (extents etc.)

        return true;
    });

    if (!found_inode_item) {
        printk("INODE_ITEM for subvol ");
        print_hex(subvol);
        printk(", inode ");
        print_hex(inode);
        printk(" not found.\n");
        return;
    }

    file_size = ii.st_size;

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

        case btrfs_inode_type::unknown:
            switch ((ii.st_mode & 0xf000) >> 12) {
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
        break;
    }

    valid = true;
}

btrfs_inode::~btrfs_inode() {
    {
        lock_guard lock(children_lock);

        while (!is_list_empty(children)) {
            auto& de = containing_record<dirent>(children.next);

            remove_entry_list(de.le);
            free(&de);
        }
    }

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

optional<reference_wrapper<btrfs_inode>> btrfs_inode::open_child(string_view name) {
    dirent* de = nullptr;

    if (inode_type != inode_type::directory)
        return nullopt;

    {
        lock_guard lock(children_lock);

        auto le = children.next;
        while (le != &children) {
            auto& de2 = containing_record<dirent>(le);

            if (de2.namelen == name.length() && string_view(de2.name, de2.namelen) == name) {
                de = &de2;
                break;
            }

            le = le->next;
        }
    }

    if (!de)
        return nullopt;

    if (de->type == key_type::ROOT_ITEM)
        return fs.open_inode(de->inode, SUBVOL_ROOT_INODE);
    else
        return fs.open_inode(subvol, de->inode);
}

optional<reference_wrapper<btrfs_inode>> btrfs_filesystem::open_inode(uint64_t subvol, uint64_t inode) {
    {
        lock_guard lock(inode_lock);

        auto le = inode_list.next;
        while (le != &inode_list) {
            auto& in = containing_record<btrfs_inode>(le);

            if (in.subvol == subvol && in.inode == inode) {
                in.acquire();
                return in;
            }

            le = le->next;
        }
    }

    auto ino = new btrfs_inode(*this, subvol, inode, btrfs_inode_type::unknown);

    if (!ino) {
        printk("new failed\n");
        return nullopt;
    }

    if (!ino->valid) {
        delete ino;
        return nullopt;
    }

    {
        optional<lock_guard<spinlock>> lock(inode_lock);

        auto le = inode_list.next;
        while (le != &inode_list) {
            auto& in = containing_record<btrfs_inode>(le);

            if (in.subvol == subvol && in.inode == inode) {
                in.acquire();
                lock.reset();

                delete ino;

                return in;
            }

            le = le->next;
        }

        insert_tail_list(inode_list, ino->le);
    }

    return *ino;
}

object_ref btrfs_filesystem::open(string_view fn, const optional<reference_wrapper<inode>>& root_dir) {
    if (!root_dir) {
        if (fn.empty() || fn[0] != '/') {
            printk("Not an absolute path.\n");
            return {};
        }

        fn.remove_prefix(1);

        if (fn.empty()) { // root
            root_inode->acquire();
            return *root_inode;
        }
    }

    // FIXME - do we need to make sure root_dir is definitely a btrfs inode?

    reference_wrapper<btrfs_inode> dirrw{!root_dir ? *root_inode : static_cast<btrfs_inode&>(root_dir->get())};
    dirrw.get().acquire();

    // FIXME - die if anything in middle not directory

    while (true) {
        auto sl = fn.find("/");
        auto& dir = dirrw.get();

        // FIXME - .. and .

        auto objrw = dir.open_child(sl == string_view::npos ? fn : string_view(fn.data(), sl));
        if (!objrw) {
            dir.release();
            return {};
        }

        auto& obj = objrw->get();

        dir.release();

        if (sl == string_view::npos)
            return obj;

        fn = string_view(fn.data() + sl + 1, fn.size() - sl - 1);
        dirrw = obj;
    }
}

size_t btrfs_inode::read_zeroes_into_cache(uint64_t page_num, uint64_t num_pages) {
    page_cache_entry* dest_p;

    // FIXME - this should be populating page cache with references to the zero page

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = ::find_page_cache_entry(cache.cache, page_num);
            else
                dest_p = &containing_record<page_cache_entry>(dest_p->le.next);
        }

        auto dest_va = phys_address(dest_p->phys_address);

        memset(dest_va, 0, PAGE_SIZE);
    }

    return num_pages;
}

size_t btrfs_inode::read_inline_extent_into_cache(uint64_t page_num,
                                                  uint64_t num_pages,
                                                  const inode_extent& ie) {
    if (ie.ed.compression != 0) {
        printk("FIXME - compression\n"); // FIXME
        return 0;
    }

    if (page_num >= bytes_to_pages(ie.ed.decoded_size))
        return 0;

    num_pages = min(num_pages, (uint64_t)bytes_to_pages(ie.ed.decoded_size) - page_num);

    if (num_pages == 0)
        return 0;

    page_cache_entry* dest_p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = ::find_page_cache_entry(cache.cache, page_num);
            else
                dest_p = &containing_record<page_cache_entry>(dest_p->le.next);
        }

        auto dest_va = phys_address(dest_p->phys_address);

        uint64_t offset = (page_num + i) << PAGE_SHIFT;
        auto to_copy = min((uint64_t)PAGE_SIZE, ie.ed.decoded_size - offset);

        memcpy(dest_va, ie.ed.data + offset, to_copy);

        memset((uint8_t*)dest_va + to_copy, 0, PAGE_SIZE - to_copy);
    }

    return num_pages;
}

size_t btrfs_inode::read_extent_into_cache(uint64_t page_num, uint64_t num_pages,
                                           const inode_extent& ie) {
    if (ie.ed.compression != 0) {
        printk("FIXME - compression\n"); // FIXME
        return 0;
    }

    const auto& ed2 = *(extent_data2*)ie.ed.data;

    uint64_t addr = ed2.address + ed2.offset + (page_num << PAGE_SHIFT) - ie.offset;

    num_pages = min(num_pages, ed2.num_bytes >> PAGE_SHIFT);

    auto pages_read = fs.read_into_cache_and_pin(addr >> PAGE_SHIFT, num_pages);
    if (pages_read == 0)
        return 0;

    // copy from buffer into our own page cache

    // FIXME - can we steal pages?

    page_cache_entry* dest_p;

    for (uint64_t i = 0; i < pages_read; i++) {
        page_cache_entry* src_p;

        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = ::find_page_cache_entry(cache.cache, page_num);
            else
                dest_p = &containing_record<page_cache_entry>(dest_p->le.next);
        }

        src_p = fs.find_page_cache_entry((addr >> PAGE_SHIFT) + i);
        // FIXME - find next entry rather than doing complete lookup

        auto src_va = phys_address(src_p->phys_address);
        auto dest_va = phys_address(dest_p->phys_address);

        memcpy(dest_va, src_va, PAGE_SIZE);
    }

    fs.unpin(addr >> PAGE_SHIFT, pages_read);

    // FIXME - check csums
    // FIXME - read from another copy (and replace) if csum is bad

    return pages_read;
}

size_t btrfs_inode::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    uint64_t lastoff = page_num << PAGE_SHIFT;

    // FIXME - extents should be an RB tree

    auto le = extents.next;
    while (le != &extents) {
        auto& ie = containing_record<inode_extent>(le);

        if (ie.offset + ie.ed.decoded_size < page_num << PAGE_SHIFT) {
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
                auto pages_to_zero = min(num_pages, ie.ed.decoded_size >> PAGE_SHIFT);
                return read_zeroes_into_cache(page_num, pages_to_zero);
            }

            default:
                printk("unknown btrfs extent type");
                return 0;
        }
    }

    return read_zeroes_into_cache(page_num, num_pages);
}

NTSTATUS btrfs_inode::read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    if (offset >= file_size)
        return nt_status::END_OF_FILE;

    if (s.empty()) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    if (offset + s.size() > file_size)
        s = s.subspan(0, file_size - offset);

    read_bytes = cache.read(s, offset);
    return nt_status::SUCCESS;
}

NTSTATUS btrfs_filesystem::query_directory_dirent(const dirent& de, span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class) {
    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::DirectoryInformation: {
            if (buf.size() < offsetof(FILE_DIRECTORY_INFORMATION, FileName))
                return nt_status::BUFFER_TOO_SMALL;

            auto sv = string_view(de.name, de.namelen);
            auto utf16len = utf8_to_utf16_len(sv);
            auto len = offsetof(FILE_DIRECTORY_INFORMATION, FileName) + (utf16len * sizeof(char16_t));

            if (buf.size() < len)
                return nt_status::BUFFER_TOO_SMALL;

            auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf.data();

            // FIXME - get info from inode

            fdi.NextEntryOffset = 0;
            fdi.FileIndex = 0;
            fdi.CreationTime.QuadPart = 0; // FIXME
            fdi.LastAccessTime.QuadPart = 0; // FIXME
            fdi.LastWriteTime.QuadPart = 0; // FIXME
            fdi.ChangeTime.QuadPart = 0; // FIXME
            fdi.EndOfFile.QuadPart = 0; // FIXME
            fdi.AllocationSize.QuadPart = 0; // FIXME
            fdi.FileAttributes = de.inode_type == btrfs_inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0; // FIXME
            fdi.FileNameLength = (uint32_t)(utf16len * sizeof(char16_t));
            utf8_to_utf16(span(fdi.FileName, utf16len), sv);

            buf = buf.subspan(0, len);

            return nt_status::SUCCESS;
        }

        default:
            printk("btrfs_filesystem::query_directory_dirent: unhandled info class\n");
            return nt_status::INVALID_INFO_CLASS;
    }
}

NTSTATUS btrfs_inode::query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index) {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    dirent* de = nullptr;

    {
        lock_guard lock(children_lock);

        auto le = children.next;
        while (le != &children) {
            auto& de2 = containing_record<dirent>(le);

            if (de2.index > index) {
                auto delen = offsetof(dirent, name) + de2.namelen;
                de = (dirent*)malloc(delen);
                if (!de)
                    return nt_status::INSUFFICIENT_RESOURCES;

                memcpy(de, &de2, delen);

                break;
            }

            le = le->next;
        }
    }

    if (!de)
        return nt_status::NO_MORE_FILES;

    auto ret = fs.query_directory_dirent(*de, buf, file_information_class);

    index = de->index;
    free(de);

    return ret;
}

uint64_t btrfs_inode::read_into_cache_and_pin(uint64_t page_num,
                                              uint64_t num_pages) {
    if (!cache.pin(page_num, num_pages, nullptr))
        return 0;

    auto read_pages = read_into_cache(page_num, num_pages);

    if (read_pages < num_pages)
        cache.unpin(page_num + read_pages, num_pages - read_pages);

    return read_pages;
}

void btrfs_inode::unpin(uint64_t page_num, uint64_t num_pages) {
    cache.unpin(page_num, num_pages);
}

page_cache_entry* btrfs_inode::find_page_cache_entry(uint64_t page_num) {
    return ::find_page_cache_entry(cache.cache, page_num);
}
