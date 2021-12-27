#include "kernel.h"
#include "objects.h"
#include "btrfs.h"

using namespace std;

static const uint64_t superblock_addrs[] = { 0x10000, 0x4000000, 0x4000000000, 0x4000000000000, 0 };

static const uint64_t supported_incompat_flags = BTRFS_INCOMPAT_FLAGS_MIXED_BACKREF | BTRFS_INCOMPAT_FLAGS_MIXED_GROUPS |
                                                 BTRFS_INCOMPAT_FLAGS_BIG_METADATA | BTRFS_INCOMPAT_FLAGS_EXTENDED_IREF |
                                                 BTRFS_INCOMPAT_FLAGS_SKINNY_METADATA | BTRFS_INCOMPAT_FLAGS_NO_HOLES;

btrfs_filesystem::btrfs_filesystem(volume& vol) : filesystem(vol) {
    vol.acquire();

    if (!vol.read(span((uint8_t*)&sb, sizeof(superblock)), superblock_addrs[0]))
        return;

    if (sb.magic != btrfs_magic)
        return;

    // FIXME - check checksum
    // FIXME - check other superblock addresses, and choose one with highest generation

    if (sb.incompat_flags & ~supported_incompat_flags) {
        print("Unsupported incompat flags\n");
        return;
    }

    if (sb.num_devices != 1) {
        print("Multiple devices not supported\n");
        return;
    }

    // FIXME - make sure sb.n < sizeof(sb.sys_chunk_array)
    auto chunk_array = span(sb.sys_chunk_array.data(), sb.n);

    while (!chunk_array.empty()) {
        if (chunk_array.size() < sizeof(key)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& k = *(key*)chunk_array.data();

        if (k.obj_type != key_type::CHUNK_ITEM) {
            print("Unexpected item in chunk array\n");
            return;
        }

        chunk_array = chunk_array.subspan(sizeof(key));

        if (chunk_array.size() < offsetof(chunk_item, stripes)) {
            print("Chunk array truncated\n");
            return;
        }

        auto& ci = *(chunk_item*)chunk_array.data();
        size_t chunk_size = offsetof(chunk_item, stripes) + (sizeof(chunk_item_stripe) * ci.num_stripes);

        if (chunk_array.size() < chunk_size) {
            print("Chunk array truncated\n");
            return;
        }

        if (ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
            print("RAID not supported\n");
            return;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            print("malloc failed\n");
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
            print("new failed\n");
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
            print("new failed\n");
            return;
        }

        auto& r = *rptr;
        r.id = BTRFS_ROOT_CHUNK;
        r.ri.block_number = sb.chunk_tree_addr;
        r.ri.root_level = sb.chunk_root_level;

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

    root_inode = new btrfs_inode(*this, BTRFS_ROOT_FSTREE, SUBVOL_ROOT_INODE);
    if (!root_inode) {
        print("new failed\n");
        return;
    }

    if (!root_inode->valid)
        return;

    insert_tail_list(inode_list, root_inode->le);

    valid = true;
}

bool btrfs_probe(volume& vol) {
    auto fs = new btrfs_filesystem(vol);

    if (!fs->valid) {
        delete fs;
        return false;
    }

    add_fs_to_list(*fs);

    return true;
}

bool btrfs_filesystem::read_metadata(uint64_t addr, span<uint8_t> data) {
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
        print("Could not find chunk for logical address ");
        print_hex(addr);
        print("\n");
        return false;
    }

    auto& c = crw->get();

    if (c.ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
        print("RAID not supported\n");
        return false;
    } else if (c.ci.type & BLOCK_FLAG_DUPLICATE) {
        print("FIXME - DUP\n");
        return false;
    } else {
        if (!read_single(addr, c, data))
            return false;
    }

    // FIXME - check checksum, address, level, and subvol ID
    // FIXME - if duff, read from other version (and replace)

    return true;
}

bool btrfs_filesystem::walk_tree2(uint64_t addr, uint8_t exp_level, invocable<const key&, span<const uint8_t>> auto func) {
    auto bufptr = (uint8_t*)malloc(sb.node_size);
    if (!bufptr) {
        print("malloc failed\n");
        return false;
    }

    auto buf = span(bufptr, sb.node_size);

    if (!read_metadata(addr, buf)) {
        print("read_metadata failed\n");
        free(bufptr);
        return false;
    }

    auto& th = *(tree_header*)buf.data();

    if (th.level != exp_level) {
        print("Tree level was incorrect\n");
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
        print("Could not find address for root ");
        print_hex(id);
        print("\n");
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
            print("Chunk item truncated\n");
            return;
        }

        auto& ci = *(chunk_item*)data.data();
        size_t chunk_size = offsetof(chunk_item, stripes) + (sizeof(chunk_item_stripe) * ci.num_stripes);

        if (data.size() < chunk_size) {
            print("Chunk array truncated\n");
            return;
        }

        if (ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
            print("RAID not supported\n");
            return;
        }

        auto cptr = (chunk*)malloc(offsetof(chunk, ci) + chunk_size);
        if (!cptr) {
            print("malloc failed\n");
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

bool btrfs_filesystem::read_single(uint64_t addr, chunk& c, span<uint8_t> data) {
    auto s = vol.read(data, addr - c.offset + c.ci.stripes[0].offset);

    if (s < data.size()) {
        print("Short read\n");
        return false;
    }

    return true;
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
            print("Root item truncated\n");
            return;
        }

        auto rptr = new root;
        if (!rptr) {
            print("new failed\n");
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
        print("malloc failed\n");
        return 0;
    }

    auto buf = span(bufptr, sb.node_size);

    if (!read_metadata(addr, buf)) {
        print("read_metadata failed\n");
        free(bufptr);
        return 0;
    }

    auto& th = *(tree_header*)buf.data();

    if (th.level != exp_level) {
        print("Tree level was incorrect\n");
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
        print("Could not find address for root ");
        print_hex(subvol);
        print("\n");
        return;
    }

    walk_from2(addr, level, k, func);
}

btrfs_inode::btrfs_inode(btrfs_filesystem& fs, uint64_t subvol, uint64_t inode) : fs(fs), subvol(subvol), inode(inode) {
    bool found_inode_item = false;

    fs.walk_from(subvol, key(inode, key_type::INODE_ITEM, 0), [&](const key& k, span<const uint8_t> data) {
        if (k.obj_id != inode)
            return false;

        switch (k.obj_type) {
            case key_type::INODE_ITEM:
                if (data.size() < sizeof(inode_item)) {
                    print("INODE_ITEM was short\n");
                    return false;
                }

                memcpy(&ii, data.data(), min(data.size(), sizeof(inode_item)));
                found_inode_item = true;
            break;

            case key_type::DIR_ITEM: {
                auto& di = *(dir_item*)data.data();

                if (data.size() < offsetof(dir_item, name)) {
                    print("DIR_ITEM was short\n");
                    return false;
                }

                if (data.size() < offsetof(dir_item, name) + di.m + di.n) {
                    print("DIR_ITEM was short\n");
                    return false;
                }

                if (di.n > 255) {
                    print("Skipping item with overly long name\n");
                    return true;
                }

                auto deptr = (dirent*)malloc(offsetof(dirent, name) + di.n);
                if (!deptr) {
                    print("malloc failed\n");
                    return false;
                }

                auto& de = *deptr;

                de.inode = di.k.obj_id;
                de.type = di.k.obj_type;
                de.namelen = (uint8_t)di.n;
                memcpy(de.name, di.name, di.n);
                insert_tail_list(children, de.le);

                break;
            }

            case key_type::EXTENT_DATA: {
                auto& ed = *(extent_data*)data.data();

                if (data.size() < offsetof(extent_data, data)) {
                    print("EXTENT_DATA was short\n");
                    return false;
                }

                if (ed.type == btrfs_extent_type::inline_extent) {
                    print("FIXME - inline extents\n"); // FIXME
                    return true;
                } else if (ed.type != btrfs_extent_type::regular || ed.type != btrfs_extent_type::prealloc) {
                    if (data.size() < offsetof(extent_data, data) + sizeof(extent_data2)) {
                        print("EXTENT_DATA was short\n");
                        return false;
                    }
                } else {
                    print("Invalid extent type\n");
                    return false;
                }

                if (ed.encryption != 0) {
                    print("Encryption not supported\n");
                    return false;
                }

                if (ed.encoding != 0) {
                    print("Other encodings not supported\n");
                    return false;
                }

                if (ed.type == btrfs_extent_type::regular) {
                    auto& ed2 = *(extent_data2*)ed.data;

                    if (ed2.address == 0 || ed2.size == 0) // skip sparse extents
                        return true;
                }

                auto ieptr = (inode_extent*)malloc(offsetof(inode_extent, ed) + data.size());
                if (!ieptr) {
                    print("malloc failed\n");
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
        print("INODE_ITEM for subvol ");
        print_hex(subvol);
        print(", inode ");
        print_hex(inode);
        print(" not found.\n");
        return;
    }

    valid = true;
}

btrfs_inode::~btrfs_inode() {
    while (!is_list_empty(children)) {
        auto& de = containing_record<dirent>(children.next);

        remove_entry_list(de.le);
        free(&de);
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

    // FIXME - fail straightaway if not directory

    auto le = children.next;
    while (le != &children) {
        auto& de2 = containing_record<dirent>(le);

        if (de2.namelen == name.length() && string_view(de2.name, de2.namelen) == name) {
            de = &de2;
            break;
        }

        le = le->next;
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

    auto ino = new btrfs_inode(*this, subvol, inode);

    if (!ino) {
        print("new failed\n");
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

unique_handle btrfs_filesystem::open(string_view fn) {
    if (fn.empty()) { // root
        auto hv = add_handle(*root_inode, 0); // FIXME - access mask

        return unique_handle{hv};
    }

    // FIXME - die if anything in middle not directory

    reference_wrapper<btrfs_inode> dirrw = *root_inode;
    dirrw.get().acquire();

    while (true) {
        auto sl = fn.find("/");
        auto& dir = dirrw.get();

        auto objrw = dir.open_child(sl == string_view::npos ? fn : string_view(fn.data(), sl));
        if (!objrw) {
            dir.release();
            return {};
        }

        auto& obj = objrw->get();

        dir.release();

        if (sl == string_view::npos) {
            auto hv = add_handle(obj, 0); // FIXME - access mask

            obj.release();

            return unique_handle{hv};
        }

        fn = string_view(fn.data() + sl + 1, fn.size() - sl - 1);
        dirrw = obj;
    }
}

size_t btrfs_filesystem::read_extent_single(chunk& c, uint64_t address, uint64_t offset, uint64_t num_bytes,
                                            span<uint8_t> s) {
    size_t to_read = min(num_bytes, s.size());

    if (offset != 0) {
        auto diff = (offset / sb.sector_size) * sb.sector_size;

        address += diff;
        offset -= diff;
    }

    // FIXME - write directly into s if possible?

    auto pages = bytes_to_pages(num_bytes + offset);
    auto va = allocate_va(pages);

    for (size_t i = 0; i < pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            print("allocate_page failed\n");

            for (size_t j = 0; j < i; j++) {
                auto pa = va_to_pa(va + (j << PAGE_SHIFT));
                unmap_page(va + (j << PAGE_SHIFT));
                free_page(pa);
            }

            return 0;
        }

        if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
            print("map_page failed\n");
            free_page(pa);

            for (size_t j = 0; j < i; j++) {
                auto pa = va_to_pa(va + (j << PAGE_SHIFT));
                unmap_page(va + (j << PAGE_SHIFT));
                free_page(pa);
            }

            return 0;
        }
    }

    auto buf = (uint8_t*)va;

    if (!read_single(address, c, span(buf, num_bytes + offset))) {
        print("btrfs_filesystem::read_single failed\n");
        to_read = 0;
        goto end;
    }

    // FIXME - check checksum

    memcpy(s.data(), buf + offset, to_read);

end:
    for (size_t i = 0; i < pages; i++) {
        auto pa = va_to_pa(va + (i << PAGE_SHIFT));
        unmap_page(va + (i << PAGE_SHIFT));
        free_page(pa);
    }

    return to_read;
}

size_t btrfs_filesystem::read_extent(uint64_t address, uint64_t offset, uint64_t num_bytes, span<uint8_t> s) {
    optional<reference_wrapper<chunk>> crw;

    {
        auto le = chunk_list.next;
        while (le != &chunk_list) {
            auto& c = containing_record<chunk>(le);

            if (address >= c.offset && address < c.offset + c.ci.size) {
                crw = c;
                break;
            }

            le = le->next;
        }
    }

    if (!crw) {
        print("Could not find chunk for address ");
        print_hex(address);
        print(".\n");
        return 0;
    }

    auto& c = crw->get();

    if (c.ci.type & (BLOCK_FLAG_RAID0 | BLOCK_FLAG_RAID1 | BLOCK_FLAG_RAID10 | BLOCK_FLAG_RAID5 | BLOCK_FLAG_RAID6 | BLOCK_FLAG_RAID1C3 | BLOCK_FLAG_RAID1C4)) {
        print("RAID not supported\n");
        return 0;
    } else if (c.ci.type & BLOCK_FLAG_DUPLICATE) {
        print("FIXME - DUP\n");
        return 0;
    } else
        return read_extent_single(c, address, offset, num_bytes, s);
}

size_t btrfs_inode::read(span<uint8_t> s, uint64_t offset) {
    uint64_t lastoff = offset;
    size_t read = 0;

    if (offset > ii.st_size || s.empty())
        return 0;

    if (offset + s.size() > ii.st_size)
        s = s.subspan(ii.st_size - offset);

    auto le = extents.next;
    while (le != &extents) {
        auto& ie = containing_record<inode_extent>(le);

        if (ie.offset + ie.ed.decoded_size < offset) {
            le = le->next;
            continue;
        }

        if (ie.ed.compression != 0) {
            print("FIXME - compression\n"); // FIXME
            return 0;
        }

        if (ie.offset > lastoff) {
            auto to_zero = min(s.size(), ie.offset - lastoff);

            memset(s.data(), 0, to_zero);
            s = s.subspan(to_zero);
            read += to_zero;

            if (s.empty())
                return read;
        }

        switch (ie.ed.type) {
            case btrfs_extent_type::inline_extent:
                print("FIXME - inline extents\n"); // FIXME
                return 0;
            break;

            case btrfs_extent_type::regular: {
                auto& ed2 = *(extent_data2*)ie.ed.data;
                auto r = fs.read_extent(ed2.address, ed2.offset, ed2.num_bytes, s);

                if (r == 0) {
                    print("btrfs_filesystem::read_extent failed\n");
                    return 0;
                }

                read += r;
                s = s.subspan(read);

                if (s.empty())
                    return read;

                break;
            }

            case btrfs_extent_type::prealloc: {
                auto to_zero = min(s.size(), ie.ed.decoded_size);

                memset(s.data(), 0, to_zero);
                s = s.subspan(to_zero);
                read += to_zero;

                if (s.empty())
                    return read;

                break;
            }
        }

        lastoff = ie.offset;

        le = le->next;
    }

    if (!s.empty()) {
        memset(s.data(), 0, s.size());
        read += s.size();
    }

    return read;
}
