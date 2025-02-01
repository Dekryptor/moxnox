#include "kernel.h"
#include "objects.h"
#include "btrfs.h"
#include "handle.h"
#include "crc32c.h"

using namespace std;

static const uint32_t S_IFREG = 0100000;
static const uint32_t S_IFDIR = 0040000;

static const uint64_t superblock_addrs[] = { 0x10000, 0x4000000, 0x4000000000, 0x4000000000000, 0 };

static const uint64_t supported_incompat_flags = BTRFS_INCOMPAT_FLAGS_MIXED_BACKREF | BTRFS_INCOMPAT_FLAGS_MIXED_GROUPS |
                                                 BTRFS_INCOMPAT_FLAGS_BIG_METADATA | BTRFS_INCOMPAT_FLAGS_EXTENDED_IREF |
                                                 BTRFS_INCOMPAT_FLAGS_SKINNY_METADATA | BTRFS_INCOMPAT_FLAGS_NO_HOLES;

static bool check_checksum(const superblock& sb) {
    auto csum = ~calc_crc32c(0xffffffff, span((const uint8_t*)&sb.uuid, sizeof(superblock) - sizeof(sb.checksum)));

    return csum == *(uint32_t*)&sb;
}

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

    // FIXME - if checksum fails, try other superblock addresses
    if (!check_checksum(sb)) {
        printk("Incorrect superblock checksum\n");
        return;
    }

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

    // FIXME - subpage on ARM?
    if (sb.sector_size != PAGE_SIZE) {
        printk("btrfs sector size is not CPU page size\n");
        return;
    }

    if (sb.csum_type != 0) {
        printk("unsupported csum type\n");
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

    if (!root_inode->load_inode())
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

nt_status btrfs_filesystem::read_metadata(uint64_t addr, span<uint8_t> data) {
    if (!cache.read(data, addr)) {
        printk("Short read\n");
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - check checksum, address, level, and subvol ID
    // FIXME - if duff, read from other version (and replace)

    return nt_status::SUCCESS;
}

bool btrfs_filesystem::load_chunks() {
    list_entry new_chunk_list = { &new_chunk_list, &new_chunk_list };

    auto r = find_root(BTRFS_ROOT_CHUNK);
    if (!r) {
        printk("Chunk root not found\n");
        return false;
    }

    btrfs_path p;

    auto status = find_item(*r, {0, key_type::CHUNK_ITEM, 0}, p);
    if (!success(status))
        return false;

    bool fail = false;

    do {
        auto& th = *(tree_header*)p.data[0];
        auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        if (p.slots[0] >= th.num_items)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.k;

        if (k.obj_type != key_type::CHUNK_ITEM) {
            if (!move_path_forward(p))
                break;

            continue;
        }

        auto data = span((const uint8_t*)&th + sizeof(tree_header) + it.offset, it.size);

        if (data.size() < offsetof(chunk_item, stripes)) {
            printk("Chunk item truncated\n");
            fail = true;
            break;
        }

        auto& ci = *(chunk_item*)data.data();
        size_t chunk_size = offsetof(chunk_item, stripes) + (sizeof(chunk_item_stripe) * ci.num_stripes);

        if (data.size() < chunk_size) {
            printk("Chunk array truncated\n");
            fail = true;
            break;
        }

        if (ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
            printk("RAID not supported\n");
            fail = true;
            break;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            printk("malloc failed\n");
            fail = true;
            break;
        }

        auto& c = *cptr;

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

    auto pages_read = vol.cache.read_into_cache_and_pin(disk_addr >> PAGE_SHIFT,
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
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = &containing_record<page_cache_entry>(dest_p->le.next);
        }

        src_p = vol.cache.find_page_cache_entry((disk_addr >> PAGE_SHIFT) + i);
        // FIXME - find next entry rather than doing complete lookup

        auto src_va = phys_address(src_p->phys_address);
        auto dest_va = phys_address(dest_p->phys_address);

        memcpy(dest_va, src_va, PAGE_SIZE);
    }

    vol.cache.unpin(disk_addr >> PAGE_SHIFT, pages_read);

    // FIXME - check csum (metadata or data)
    // FIXME - choose other copy if csum wrong
    // FIXME - copy over bad copy if csum wrong

    return pages_read;
}

bool btrfs_filesystem::flush_tree(const chunk& c, uint64_t addr) {
    unsigned int pages_per_tree = sb.node_size >> PAGE_SHIFT;

    // FIXME - calculate hashes etc.

    if (c.ci.type & BLOCK_FLAG_RAID0) {
        printk("RAID0 not supported\n");
        return false;
    } else if (c.ci.type & BLOCK_FLAG_RAID10) {
        printk("RAID10 not supported\n");
        return false;
    } else if (c.ci.type & BLOCK_FLAG_RAID5) {
        printk("RAID5 not supported\n");
        return false;
    } else if (c.ci.type & BLOCK_FLAG_RAID6) {
        printk("RAID6 not supported\n");
        return false;
    }

    // chunk is SINGLE, DUP, RAID1, RAID1C3, or RAID1C4

    // FIXME - handle missing devices

    // copy into page cache of volumes

    cache.pin(addr >> PAGE_SHIFT, pages_per_tree, nullptr);

    auto* first_page = cache.find_page_cache_entry(addr >> PAGE_SHIFT);
    if (!first_page) {
        printk("cache.find_page_cache_entry returned NULL\n");
        return false;
    }

    for (unsigned int i = 0; i < c.ci.num_stripes; i++) {
        auto ent = first_page;
        uint64_t disk_addr = addr - c.offset + c.ci.stripes[i].offset;

        for (unsigned j = 0; j < pages_per_tree; j++) {
            assert(ent->page_num == (addr >> PAGE_SHIFT) + j);

            if (ent->status == page_cache_status::dirty) {
                if (!vol.cache.write_pages(disk_addr >> PAGE_SHIFT, 1,
                    span((uint8_t*)phys_address(ent->phys_address), PAGE_SIZE))) {
                    printk("volume::write_pages failed\n");
                    cache.unpin(addr >> PAGE_SHIFT, pages_per_tree);
                    return false;
                }
            }

            disk_addr += PAGE_SIZE;

            if (j != pages_per_tree - 1)
                ent = &containing_record<page_cache_entry>(ent->le.next);
        }
    }

    auto ent = first_page;
    for (unsigned j = 0; j < pages_per_tree; j++) {
        if (ent->status == page_cache_status::dirty)
            ent->status = page_cache_status::clean;

        if (j != pages_per_tree - 1)
            ent = &containing_record<page_cache_entry>(ent->le.next);
    }

    cache.unpin(addr >> PAGE_SHIFT, pages_per_tree);

    return true;
}

size_t btrfs_filesystem::flush_cache(uint64_t page_num, uint64_t num_pages) {
    unsigned int pages_per_tree = sb.node_size >> PAGE_SHIFT;
    uint64_t addr = page_num << PAGE_SHIFT;

    if (page_num % pages_per_tree) // not aligned to tree
        return 0;

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
    uint64_t pages_written = 0;

    while (addr < c.ci.size) {
        if (!flush_tree(c, addr))
            break;

        addr += sb.node_size;
        pages_written += pages_per_tree;

        if (pages_written >= num_pages)
            break;
    }

    if (pages_written == 0)
        return pages_written;

    uint64_t pages_to_flush = pages_written;
    for (unsigned int i = 0; i < c.ci.num_stripes; i++) {
        uint64_t disk_addr = (page_num << PAGE_SHIFT) - c.offset + c.ci.stripes[i].offset;
        uint64_t first_page = disk_addr >> PAGE_SHIFT;

        while (true) {
            auto flushed = vol.flush_cache(first_page, pages_to_flush);

            if (flushed == 0) {
                printk("volume::flush_cache failed\n");
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

bool btrfs_filesystem::load_roots() {
    list_entry new_root_list = { &new_root_list, &new_root_list };

    auto r = find_root(BTRFS_ROOT_ROOT);
    if (!r) {
        printk("Root root not found\n");
        return false;
    }

    btrfs_path p;

    auto status = find_item(*r, {0, key_type::ROOT_ITEM, 0}, p);
    if (!success(status))
        return false;

    bool fail = false;

    do {
        auto& th = *(tree_header*)p.data[0];
        auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        if (p.slots[0] >= th.num_items)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.k;

        if (k.obj_type != key_type::ROOT_ITEM) {
            if (!move_path_forward(p))
                break;

            continue;
        }

        auto data = span((const uint8_t*)&th + sizeof(tree_header) + it.offset, it.size);

        if (data.size() < sizeof(root_item)) {
            printk("Root item truncated\n");
            fail = true;
            break;
        }

        auto rptr = new root;
        if (!rptr) {
            printk("new failed\n");
            fail = true;
            break;
        }

        auto& r = *rptr;

        r.id = k.obj_id;
        memcpy(&r.ri, data.data(), sizeof(root_item));
        insert_tail_list(new_root_list, r.le);

        if (!move_path_forward(p))
            break;
    } while (true);

    if (fail) {
        swap_lists(root_list, new_root_list);
        free_root_list();
        swap_lists(root_list, new_root_list);
        return false;
    }

    free_root_list();
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
    btrfs_path p;

    status = fs.find_item(*r, {inode, key_type::INODE_ITEM, 0}, p);
    if (!success(status)) // FIXME - return this somehow
        return false;

    do {
        auto& th = *(tree_header*)p.data[0];
        auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        if (p.slots[0] >= th.num_items)
            break;

        const auto& it = items[p.slots[0]];
        const auto& k = it.k;

        if (k.obj_id != inode)
            break;

        auto data = span((const uint8_t*)&th + sizeof(tree_header) + it.offset, it.size);

        switch (k.obj_type) {
            case key_type::INODE_ITEM:
                if (data.size() < sizeof(inode_item)) {
                    printk("INODE_ITEM was short\n");
                    break;
                }

                memcpy(&ii, data.data(), min(data.size(), sizeof(inode_item)));
                found_inode_item = true;
            break;

            case key_type::EXTENT_DATA: {
                auto& ed = *(extent_data*)data.data();

                if (data.size() < offsetof(extent_data, data)) {
                    printk("EXTENT_DATA was short\n");
                    break;
                }

                if (ed.compression != 0) {
                    printk("Compression not supported\n");
                    break;
                }

                if (ed.encryption != 0) {
                    printk("Encryption not supported\n");
                    break;
                }

                if (ed.encoding != 0) {
                    printk("Other encodings not supported\n");
                    break;
                }

                if (ed.type == btrfs_extent_type::regular || ed.type == btrfs_extent_type::prealloc) {
                    if (data.size() < offsetof(extent_data, data) + sizeof(extent_data2)) {
                        printk("EXTENT_DATA was short\n");
                        break;
                    }
                } else if (ed.type == btrfs_extent_type::inline_extent) {
                    if (data.size() < offsetof(extent_data, data) + ed.decoded_size) {
                        printk("EXTENT_DATA was short\n");
                        break;
                    }
                } else {
                    printk("Invalid extent type\n");
                    break;
                }

                if (ed.type == btrfs_extent_type::regular) {
                    auto& ed2 = *(extent_data2*)ed.data;

                    if (ed2.address == 0 || ed2.size == 0) // skip sparse extents
                        break;
                }

                auto ieptr = (inode_extent*)malloc(offsetof(inode_extent, ed) + data.size());
                if (!ieptr) {
                    printk("malloc failed\n");
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
        printk("INODE_ITEM for subvol ");
        print_hex(r->id);
        printk(", inode ");
        print_hex(inode);
        printk(" not found.\n");
        return false;
    }

    file_size = ii.st_size;

    if (this->inode_type == inode_type::unknown) {
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

nt_status btrfs_filesystem::find_item(root& r, const key& k, btrfs_path& p) {
    uint64_t addr = r.ri.block_number;
    uint8_t level = r.ri.root_level;
    nt_status status;

    // FIXME - error if level >= BTRFS_MAX_LEVEL

    for (unsigned int i = 0; i <= level; i++) {
        p.data[i] = (uint8_t*)malloc(sb.node_size);
        if (!p.data[i])
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    do {
        status = read_metadata(addr, span(p.data[level], sb.node_size));
        if (!success(status))
            return status;

        auto& th = *(tree_header*)p.data[level];

        if (th.level != level) {
            char s[100];
            auto sp = span(s, sizeof(s));

            sp = sp_strcpy(sp, "Tree level was incorrect (");
            sp = sp_itoa_hex(sp, th.level);
            sp = sp_strcpy(sp, ", expected ");
            sp = sp_itoa_hex(sp, level);
            sp = sp_strcpy(sp, ")\n");
            printk(string_view(s, sp.data() - s));

            return nt_status::DISK_CORRUPT_ERROR;
        }

        if (th.level == 0) {
            auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

            p.slots[level] = (uint32_t)items.size();

            for (uint32_t i = 0; i < items.size(); i++) {
                if (items[i].k >= k) {
                    p.slots[level] = i;
                    break;
                }
            }
        } else {
            auto items = span((internal_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

            // FIXME
    //         for (const auto& i : items) {
    //             if (!walk_tree2(i.address, exp_level - 1, func))
    //                 break;
    //         }
        }
    } while (false); // FIXME - do loop if level > 0

    return nt_status::SUCCESS;
}

nt_status btrfs_filesystem::insert_item(root& r, const key& k,
                                        span<const uint8_t> data) {
    btrfs_path p;
    nt_status status;

    // check if data too big for any tree
    if (data.size() >= sb.node_size - sizeof(tree_header) - sizeof(leaf_node))
        return nt_status::INTERNAL_ERROR;

    status = find_item(r, k, p);
    if (!success(status))
        return status;

    auto& th = *(tree_header*)p.data[0];
    auto* items = (leaf_node*)((uint8_t*)&th + sizeof(tree_header));

    if (p.slots[0] < th.num_items && items[p.slots[0]].k == k)
        return nt_status::OBJECT_NAME_COLLISION;

    unsigned int data_size = sizeof(tree_header);

    for (unsigned int i = 0; i < th.num_items; i++) {
        data_size += items[i].size;
    }

    unsigned int size_used = data_size + (unsigned int)(th.num_items * sizeof(leaf_node));

    if (size_used + sizeof(leaf_node) + data.size() > sb.node_size) {
        printk("FIXME - split tree\n"); // FIXME
        return nt_status::NOT_SUPPORTED;
    }

    // insert new leaf_node

    if (p.slots[0] < th.num_items) {
        memmove(&items[p.slots[0] + 1], &items[p.slots[0]],
                (th.num_items - p.slots[0]) * sizeof(leaf_node));
    }

    th.num_items++;

    items[p.slots[0]].k = k;

    if (!data.empty()) {
        unsigned int to_move = 0;

        // move data around

        for (unsigned int i = p.slots[0] + 1; i < th.num_items; i++) {
            to_move += items[i].size;
            items[i].offset -= (uint32_t)data.size();
        }

        memmove(p.data[0] + sizeof(tree_header) + items[th.num_items - 1].offset - data.size(),
                p.data[0] + sizeof(tree_header) + items[th.num_items - 1].offset,
                to_move);
    }

    if (p.slots[0] == th.num_items - 1)
        items[p.slots[0]].offset = (uint32_t)(sb.node_size - sizeof(tree_header) - data.size());
    else
        items[p.slots[0]].offset = (uint32_t)(items[p.slots[0] + 1].offset + items[p.slots[0] + 1].size);

    items[p.slots[0]].size = (uint32_t)data.size();

    memcpy(p.data[0] + sizeof(tree_header) + items[p.slots[0]].offset,
           data.data(), data.size());

    // copy tree back into page cache and mark dirty
    // FIXME - when do we recalc hash?
    if (!cache.write(span(p.data[0], sb.node_size), th.address)) // FIXME - return status
        return nt_status::INTERNAL_ERROR;

    // FIXME - COWing tree (acquire new metadata address, update tree 2)

    // FIXME - split tree if would be too full, or no room

    return nt_status::SUCCESS;
}

bool btrfs_filesystem::move_path_back(btrfs_path& p) {
    // FIXME - handle moving back through internal trees

    if (p.slots[0] == 0)
        return false;

    p.slots[0]--;

    return true;
}

bool btrfs_filesystem::move_path_forward(btrfs_path& p) {
    // FIXME - handle moving forward through internal trees

    const auto& th = *(tree_header*)p.data[0];

    if (th.num_items == 0 || p.slots[0] == th.num_items - 1)
        return false;

    p.slots[0]++;

    return true;
}

nt_status btrfs_inode::get_last_dir_index(uint64_t& ret) {
    nt_status status;
    btrfs_path p;

    status = fs.find_item(*r, {inode, key_type::DIR_INDEX, 0xffffffffffffffff}, p);
    if (!success(status))
        return status;

    if (!fs.move_path_back(p)) {
        ret = 1;
        return nt_status::SUCCESS;
    }

    auto& th = *(tree_header*)p.data[0];
    auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

    if (items[p.slots[0]].k.obj_id != inode || items[p.slots[0]].k.obj_type != key_type::DIR_INDEX) {
        ret = 1;
        return nt_status::SUCCESS;
    }

    ret = items[p.slots[0]].k.offset;

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::create_child(string_view name,
                                    uint32_t create_options,
                                    optional<reference_wrapper<btrfs_inode>>& obj) {
    nt_status status;
    inode_item ii;
    bool is_dir = create_options & FILE_DIRECTORY_FILE;

    // FIXME - check name valid (not too long, not empty, not . or .., no forbidden characters)

    uint64_t new_inode = 42; // FIXME - get new inode number

    // create INODE_ITEM

    memset(&ii, 0, sizeof(inode_item));
    ii.generation = 0; // FIXME
    ii.transid = 0; // FIXME
    ii.st_nlink = 1;
    ii.st_uid = 0; // FIXME
    ii.st_gid = 0; // FIXME

    // FIXME - do we inherit mode from parent?
    if (is_dir)
        ii.st_mode = S_IFDIR | 0755;
    else
        ii.st_mode = S_IFREG | 0644;

    ii.sequence = 0; // FIXME?
    // ii.st_atime; // FIXME
    // ii.st_ctime; // FIXME
    // ii.st_mtime; // FIXME
    // ii.otime; // FIXME

    status = fs.insert_item(*r, {new_inode, key_type::INODE_ITEM, 0},
                            span((uint8_t*)&ii, sizeof(ii)));
    if (!success(status)) {
        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;
        return status;
    }

    // create DIR_ITEM in parent

    auto hash = calc_crc32c(0xfffffffe, name);

    size_t di_size = offsetof(dir_item, name) + name.size();
    auto di = (dir_item*)malloc(di_size);
    if (!di)
        return nt_status::INSUFFICIENT_RESOURCES;

    di->k.obj_id = new_inode;
    di->k.obj_type = key_type::INODE_ITEM;
    di->k.offset = 0;
    di->transid = 0; // FIXME
    di->m = 0;
    di->n = (uint16_t)name.size();
    di->type = is_dir ? btrfs_inode_type::directory : btrfs_inode_type::file;
    memcpy(di->name, name.data(), name.size());

    status = fs.insert_item(*r, {inode, key_type::DIR_ITEM, hash}, span((uint8_t*)di, di_size));

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

    status = fs.insert_item(*r, {inode, key_type::DIR_INDEX, index}, span((uint8_t*)di, di_size));

    if (!success(status)) {
        free(di);

        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;

        return status;
    }

    free(di);

    // create INODE_REF

    size_t ir_size = offsetof(inode_ref, name) + name.size();
    auto ir = (inode_ref*)malloc(ir_size);
    if (!ir)
        return nt_status::INSUFFICIENT_RESOURCES;

    ir->index = index;
    ir->n = (uint16_t)name.size();
    memcpy(ir->name, name.data(), name.size());

    status = fs.insert_item(*r, {new_inode, key_type::INODE_REF, inode},
                            span((uint8_t*)ir, ir_size));

    if (!success(status)) {
        free(ir);

        if (status == nt_status::OBJECT_NAME_COLLISION)
            status = nt_status::INTERNAL_ERROR;

        return status;
    }

    free(ir);

    // FIXME - adjust size of parent directory

    // create btrfs_inode object and return

    auto bi = new btrfs_inode(fs, r->id, new_inode,
                              is_dir ? btrfs_inode_type::directory : btrfs_inode_type::file);

    memcpy(&bi->ii, &ii, sizeof(inode_item));
    bi->file_size = 0;

    {
        lock_guard lock(fs.inode_lock);
        insert_tail_list(fs.inode_list, bi->le);
    }

    obj = *bi;

    return nt_status::SUCCESS;
}

nt_status btrfs_inode::open_child(string_view name, uint32_t create_disposition,
                                  uint32_t create_options,
                                  optional<reference_wrapper<btrfs_inode>>& obj) {
    if (inode_type != inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    auto hash = calc_crc32c(0xfffffffe, name);

    // FIXME - transaction lock (RO if FILE_OPEN, RW otherwise)

    optional<key> dir_item_key;

    do {
        auto k = key{inode, key_type::DIR_ITEM, hash};
        btrfs_path p;

        auto status = fs.find_item(*r, k, p);
        if (!success(status))
            return status;

        auto& th = *(tree_header*)p.data[0];
        auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        if (p.slots[0] >= th.num_items)
            break;

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.k;

        if (found_key != k)
            break;

        auto data = span((const uint8_t*)&th + sizeof(tree_header) + it.offset, it.size);

        while (data.size() >= offsetof(dir_item, name)) {
            const auto& di = *(dir_item*)data.data();

            if (data.size() < offsetof(dir_item, name) + di.m + di.n)
                break;

            if (string_view(di.name, di.n) == name) {
                dir_item_key = di.k;
                break;
            }

            data = data.subspan(offsetof(dir_item, name) + di.m + di.n,
                                data.size() - offsetof(dir_item, name) - di.m - di.n);
        }
    } while (false);

    // FIXME - iosb Information

    if (dir_item_key.has_value()) {
        if (create_disposition == FILE_CREATE)
            return nt_status::OBJECT_NAME_COLLISION;

        if (create_disposition == FILE_OVERWRITE) {
            printk("FIXME - FILE_OVERWRITE\n");
            return nt_status::NOT_SUPPORTED;
        } else if (create_disposition == FILE_OVERWRITE_IF) {
            printk("FIXME - FILE_OVERWRITE_IF\n");
            return nt_status::NOT_SUPPORTED;
        } else if (create_disposition == FILE_SUPERSEDE) {
            printk("FIXME - FILE_SUPERSEDE\n");
            return nt_status::NOT_SUPPORTED;
        }

        auto& v = *dir_item_key;

        if (v.obj_type == key_type::ROOT_ITEM)
            return fs.open_inode(v.obj_id, SUBVOL_ROOT_INODE, obj);
        else
            return fs.open_inode(r->id, v.obj_id, obj);
    } else {
        if (create_disposition == FILE_OPEN || create_disposition == FILE_OVERWRITE)
            return nt_status::OBJECT_NAME_NOT_FOUND;

        auto status = create_child(name, create_options, obj);

        if (!success(status))
            return status;

        fs.do_flush(); // FIXME

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
        printk("new failed\n");
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
                                 uint32_t create_options) {
    if (!root_dir) {
        if (fn.empty() || fn[0] != '/') {
            printk("Not an absolute path.\n");
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

    // FIXME - die if anything in middle not directory

    while (true) {
        auto sl = fn.find("/");
        auto& dir = dirrw.get();
        bool last_bit = sl == string_view::npos;
        optional<reference_wrapper<btrfs_inode>> objrw;

        // FIXME - .. and .

        auto status = dir.open_child(last_bit ? fn : string_view(fn.data(), sl),
                                     last_bit ? create_disposition : FILE_OPEN,
                                     create_options, objrw);
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
}

size_t btrfs_inode::read_zeroes_into_cache(uint64_t page_num, uint64_t num_pages) {
    page_cache_entry* dest_p;

    // FIXME - this should be populating page cache with references to the zero page

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                dest_p = cache.find_page_cache_entry(page_num);
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
                dest_p = cache.find_page_cache_entry(page_num);
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

    auto pages_read = fs.cache.read_into_cache_and_pin(addr >> PAGE_SHIFT,
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
                dest_p = cache.find_page_cache_entry(page_num);
            else
                dest_p = &containing_record<page_cache_entry>(dest_p->le.next);
        }

        src_p = fs.cache.find_page_cache_entry((addr >> PAGE_SHIFT) + i);
        // FIXME - find next entry rather than doing complete lookup

        auto src_va = phys_address(src_p->phys_address);
        auto dest_va = phys_address(dest_p->phys_address);

        memcpy(dest_va, src_va, PAGE_SIZE);
    }

    fs.cache.unpin(addr >> PAGE_SHIFT, pages_read);

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

size_t btrfs_inode::flush_cache(uint64_t page_num, uint64_t num_pages) {
    printk("FIXME - btrfs_inode::flush_cache\n"); // FIXME

    return 0;
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

static NTSTATUS query_directory_dirent(string_view sv, enum btrfs_inode_type inode_type,
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

            // FIXME - get info from inode

            fdi.NextEntryOffset = 0;
            fdi.FileIndex = 0;
            fdi.CreationTime.QuadPart = 0; // FIXME
            fdi.LastAccessTime.QuadPart = 0; // FIXME
            fdi.LastWriteTime.QuadPart = 0; // FIXME
            fdi.ChangeTime.QuadPart = 0; // FIXME
            fdi.EndOfFile.QuadPart = 0; // FIXME
            fdi.AllocationSize.QuadPart = 0; // FIXME
            fdi.FileAttributes = inode_type == btrfs_inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0; // FIXME
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

    char name[256]; // FIXME - reference page cache directly
    size_t name_len;
    key dir_index_key;
    enum btrfs_inode_type inode_type;
    uint64_t found_index;
    bool found = false;

    do {
        auto k = key{inode, key_type::DIR_INDEX, index};
        btrfs_path p;

        // FIXME - transaction lock

        auto status = fs.find_item(*r, k, p);
        if (!success(status))
            return status;

        auto& th = *(tree_header*)p.data[0];
        auto items = span((leaf_node*)((uint8_t*)&th + sizeof(tree_header)), th.num_items);

        if (p.slots[0] >= th.num_items)
            break;

        const auto& it = items[p.slots[0]];
        const auto& found_key = it.k;

        if (found_key.obj_id != k.obj_id || found_key.obj_type != k.obj_type)
            break;

        if (found_key.offset < index)
            break;

        auto data = span((const uint8_t*)&th + sizeof(tree_header) + it.offset, it.size);

        // FIXME - return actual error if data malformed

        if (data.size() < offsetof(dir_item, name))
            break;

        const auto& di = *(dir_item*)data.data();

        if (data.size() < offsetof(dir_item, name) + di.m + di.n)
            break;

        if (di.n > sizeof(name))
            break;

        memcpy(name, di.name, di.n);
        name_len = di.n;

        dir_index_key = di.k;
        inode_type = di.type;
        found_index = found_key.offset;

        found = true;
    } while (false);

    if (!found)
        return nt_status::NO_MORE_FILES;

    auto ret = query_directory_dirent(string_view(name, name_len),
                                      inode_type, buf,
                                      file_information_class);

    index = found_index + 1;

    return ret;
}

void btrfs_filesystem::do_flush() {
    page_cache_entry* ent = nullptr;

    // FIXME - should have rwsem held exclusively

    // loop through cache, and call flush_cache for dirty ranges

    while (true) {
        page_cache_entry* start;
        page_cache_entry* end;
        uint64_t range_length;

        if (!cache.find_dirty_range(start, end, range_length, ent))
            break;

        auto first_page = start->page_num;
        auto num_pages = end->page_num - start->page_num + 1;

        while (true) {
            auto flushed = flush_cache(first_page, num_pages);

            if (flushed == 0) {
                printk("flush_cache did not flush any pages\n");
                return;
            }

            if (num_pages >= flushed)
                break;

            first_page += flushed;
            num_pages -= flushed;
        }
    }

    // FIXME - flush cache for block devices

    // FIXME - write superblock into cache of block devices
    // FIXME - flush block device caches again
}
