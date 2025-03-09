module;

#include "kernel.h"
#include "objects.h"
#include "handle.h"
#include "volume.h"

export module tmpfs;

import print;

using namespace std;

export class tmpfs_filesystem;

class tmpfs_inode : public inode {
public:
    tmpfs_inode(tmpfs_filesystem& fs) : fs(fs) { }
    void release() override;

    nt_status open_child(string_view name, uint32_t create_disposition,
                         uint32_t create_options,
                         optional<reference_wrapper<tmpfs_inode>>& obj);
    nt_status create_child(string_view name,
                           uint32_t create_options,
                           optional<reference_wrapper<tmpfs_inode>>& obj);
    NTSTATUS read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) override;
    NTSTATUS query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;

    list_entry le;

private:
    tmpfs_filesystem& fs;
};

class tmpfs_filesystem : public filesystem {
public:
    tmpfs_filesystem();

    ~tmpfs_filesystem() override {
        if (root_inode)
            root_inode->release();
    }

    nt_status open(string_view fn, const optional<reference_wrapper<inode>>& dir,
                   object_ref& obj_ref, uint32_t create_disposition,
                   uint32_t create_options) override;
    nt_status flush(bool make_readonly) override;

    tmpfs_inode* root_inode = nullptr;
    spinlock inode_lock;
    list_entry inode_list = { &inode_list, &inode_list };

private:
    bool readonly = false;
};

tmpfs_filesystem::tmpfs_filesystem() : filesystem(nullptr) {
    root_inode = new tmpfs_inode(*this);
    if (!root_inode) {
        print("new failed\n");
        return;
    }

    insert_tail_list(inode_list, root_inode->le);
}

export tmpfs_filesystem* create_tmpfs() {
    auto fs = new tmpfs_filesystem();
    if (!fs)
        return nullptr;

    fs->refcount++;

    return fs;
}

nt_status tmpfs_filesystem::open(string_view fn,
                                 const optional<reference_wrapper<inode>>& root_dir,
                                 object_ref& obj_ref,
                                 uint32_t create_disposition,
                                 uint32_t create_options) {
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

    reference_wrapper<tmpfs_inode> dirrw{!root_dir ? *root_inode : static_cast<tmpfs_inode&>(root_dir->get())};
    dirrw.get().acquire();

    // FIXME - die if anything in middle not directory

    while (true) {
        auto sl = fn.find("/");
        auto& dir = dirrw.get();
        bool last_bit = sl == string_view::npos;
        optional<reference_wrapper<tmpfs_inode>> objrw;

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

nt_status tmpfs_filesystem::flush(bool make_readonly) {
    if (make_readonly)
        readonly = true;

    return nt_status::SUCCESS;
}

void tmpfs_inode::release() {
    {
        lock_guard lock(fs.inode_lock);

        if (--refcount != 0)
            return;

        remove_entry_list(le);
    }

    delete this;
}

NTSTATUS tmpfs_inode::read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    print("FIXME - tmpfs_inode::read\n");

    return nt_status::NOT_IMPLEMENTED;
}

NTSTATUS tmpfs_inode::query_directory(span<uint8_t> buf,
                                      FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index)  {
    print("FIXME - tmpfs_inode::query_directory\n");

    return nt_status::NOT_IMPLEMENTED;
}

size_t tmpfs_inode::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    print("FIXME - tmpfs_inode::read_into_cache\n");

    return 0;
}

size_t tmpfs_inode::flush_cache(uint64_t page_num, uint64_t num_pages) {
    print("FIXME - tmpfs_inode::flush_cache\n");

    return 0;
}

nt_status tmpfs_inode::open_child(string_view name, uint32_t create_disposition,
                                 uint32_t create_options,
                                 optional<reference_wrapper<tmpfs_inode>>& obj) {
    print("FIXME - tmpfs_inode::open_child\n");

    return nt_status::NOT_IMPLEMENTED;
}
