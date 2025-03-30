module;

#include "kernel.h"
import objects;
#include "handle.h"
#include "spinlock.h"
import rbtree;
#include "inode.h"

export module tmpfs;

import bsod;
import print;
import mem;
import config;
import list;
import string;
import volume;
import timer;

using namespace std;

static const size_t NAME_MAX_LEN = 255;

export class tmpfs_filesystem;

export class tmpfs_inode : public inode {
public:
    tmpfs_inode(tmpfs_filesystem& fs, enum inode_type inode_type) : fs(fs) {
        this->inode_type = inode_type;
    }

    ~tmpfs_inode();
    void release() override;

    nt_status open_child(string_view name, uint32_t create_disposition,
                         uint32_t create_options, unsigned int& create_result,
                         optional<reference_wrapper<tmpfs_inode>>& obj);
    NTSTATUS query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    NTSTATUS set_information(span<const uint8_t> buffer,
                             FILE_INFORMATION_CLASS file_information_class) override;
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status set_basic_information(std::span<const uint8_t> buffer);

    list_entry le;
    spinlock dentry_lock;
    list_entry children = { &children, &children };
    uint64_t mtime, ctime, otime, atime;

private:
    tmpfs_filesystem& fs;
};

struct dentry {
    class tmpfs_inode* inode;
    list_entry le;
    unsigned int index;
    size_t name_len;
    char name[0];
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
                   uint32_t create_options, unsigned int& create_result) override;
    nt_status flush(bool make_readonly) override;

    tmpfs_inode* root_inode = nullptr;
    spinlock inode_lock;
    list_entry inode_list = { &inode_list, &inode_list };
    bool readonly = false;
};

tmpfs_filesystem::tmpfs_filesystem() : filesystem(nullptr) {
    root_inode = new tmpfs_inode(*this, inode_type::directory);
    if (!root_inode) {
        print("new failed\n");
        return;
    }

    auto cur_time = (uint64_t)system_time + system_time_offset;

    root_inode->atime = cur_time;
    root_inode->ctime = cur_time;
    root_inode->mtime = cur_time;
    root_inode->otime = cur_time;

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

    reference_wrapper<tmpfs_inode> dirrw{!root_dir ? *root_inode : static_cast<tmpfs_inode&>(root_dir->get())};
    dirrw.get().acquire();

    // FIXME - die if anything in middle not directory

    while (true) {
        auto sl = fn.find("/");
        auto& dir = dirrw.get();
        bool last_bit = sl == string_view::npos;
        optional<reference_wrapper<tmpfs_inode>> objrw;
        unsigned int result;

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

tmpfs_inode::~tmpfs_inode() {
    lock_guard lock(dentry_lock);

    while (!is_list_empty(children)) {
        auto& c = containing_record<dentry>(children.next);

        c.inode->release();

        remove_entry_list(c.le);
        free(&c);
    }
}

static nt_status query_directory_dirent(const dentry& d, span<uint8_t> buf,
                                        FILE_INFORMATION_CLASS file_information_class) {
    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::DirectoryInformation: {
            if (buf.size() < offsetof(FILE_DIRECTORY_INFORMATION, FileName))
                return nt_status::BUFFER_TOO_SMALL;

            auto sv = string_view(d.name, d.name_len);
            auto utf16len = utf8_to_utf16_len(sv);
            auto len = offsetof(FILE_DIRECTORY_INFORMATION, FileName) + (utf16len * sizeof(char16_t));

            if (buf.size() < len)
                return nt_status::BUFFER_TOO_SMALL;

            auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf.data();

            // FIXME - should be holding lock for values in inode(?)

            fdi.NextEntryOffset = 0;
            fdi.FileIndex = 0;
            fdi.CreationTime.QuadPart = (int64_t)d.inode->otime;
            fdi.LastAccessTime.QuadPart = (int64_t)d.inode->atime;
            fdi.LastWriteTime.QuadPart = (int64_t)d.inode->mtime;
            fdi.ChangeTime.QuadPart = (int64_t)d.inode->ctime;
            fdi.EndOfFile.QuadPart = (int64_t)d.inode->file_size;
            fdi.AllocationSize.QuadPart = 0; // FIXME
            fdi.FileAttributes = d.inode->inode_type == inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0;
            fdi.FileNameLength = (uint32_t)(utf16len * sizeof(char16_t));
            utf8_to_utf16(span(fdi.FileName, utf16len), sv);

            buf = buf.subspan(0, len);

            return nt_status::SUCCESS;
        }

        default:
            print("tmpfs query_directory_dirent: unhandled info class {:x}\n",
                  (unsigned int)file_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

NTSTATUS tmpfs_inode::query_directory(span<uint8_t> buf,
                                      FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index)  {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    lock_guard lock(dentry_lock);

    // FIXME - children should be RB tree rather than linked list
    for (auto le = children.next; le != &children; le = le->next) {
        auto& d = containing_record<dentry>(le);

        if (d.index < index)
            continue;

        auto ret = query_directory_dirent(d, buf, file_information_class);

        index = d.index + 1;

        return ret;
    }

    return nt_status::NO_MORE_FILES;
}

size_t tmpfs_inode::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                ent = cache.find_page_cache_entry(page_num);
            else
                ent = cache.next_page_cache_entry(*ent);
        }

        assert(ent->tree.key == page_num + i);

        if (ent->status == page_cache_status::allocated) {
            auto ent_va = get_phys_address(*ent);

            memset(ent_va, 0, PAGE_SIZE);

            ent->status = page_cache_status::clean;
        }
    }

    return num_pages;
}

size_t tmpfs_inode::flush_cache(uint64_t, uint64_t) {
    return 0;
}

// FIXME - make this global?
static bool invalid_name(string_view name) {
    if (name.empty())
        return false;

    if (name.size() > NAME_MAX_LEN)
        return false;

    if (name == "." || name == "..")
        return false;

    for (auto c : name) {
        if (c == '/' || c == '\\' || c == 0)
            return false;
    }

    // FIXME - check is valid UTF-8?

    return true;
}

nt_status tmpfs_inode::open_child(string_view name, uint32_t create_disposition,
                                 uint32_t create_options, unsigned int& create_result,
                                 optional<reference_wrapper<tmpfs_inode>>& obj) {
    if (inode_type != inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    lock_guard lock(dentry_lock);

    for (auto le = children.next; le != &children; le = le->next) {
        auto& c = containing_record<dentry>(le);
        string_view child_name(c.name, c.name_len);

        if (name == child_name) {
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

            c.inode->acquire();
            obj = *c.inode;

            return nt_status::SUCCESS;
        }
    }

    if (create_disposition == FILE_OPEN || create_disposition == FILE_OVERWRITE) {
        create_result = FILE_DOES_NOT_EXIST;
        return nt_status::OBJECT_NAME_NOT_FOUND;
    }

    if (!invalid_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    bool is_dir = create_options & FILE_DIRECTORY_FILE;

    auto ino = new tmpfs_inode(fs, is_dir ? inode_type::directory : inode_type::file);
    if (!ino)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto c = (dentry*)malloc(offsetof(dentry, name) + name.size());
    if (!c) {
        ino->release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto cur_time = (uint64_t)system_time + system_time_offset;

    new (c) dentry;

    ino->acquire();

    ino->file_size = 0;
    ino->atime = cur_time;
    ino->ctime = cur_time;
    ino->mtime = cur_time;
    ino->otime = cur_time;

    c->inode = ino;
    c->name_len = name.size();
    memcpy(c->name, name.data(), name.size());

    if (is_list_empty(children))
        c->index = 0;
    else
        c->index = containing_record<dentry>(children.prev).index + 1;

    insert_tail_list(children, c->le);

    obj = *ino;

    create_result = FILE_CREATED;

    return nt_status::SUCCESS;
}

NTSTATUS tmpfs_inode::set_information(span<const uint8_t> buffer,
                                      FILE_INFORMATION_CLASS file_information_class) {
    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::BasicInformation:
            return set_basic_information(buffer);

        default:
            print("tmpfs_inode::set_information: unhandle file information class {}\n",
                  (unsigned int)file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status tmpfs_inode::set_basic_information(std::span<const uint8_t> buffer) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    const auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    // FIXME - setting times to -1 or -2
    // FIXME - subsequent writes etc. shouldn't clobber set time

    if (fbi.CreationTime.QuadPart != 0)
        otime = (uint64_t)fbi.CreationTime.QuadPart;

    if (fbi.LastAccessTime.QuadPart != 0)
        atime = (uint64_t)fbi.LastAccessTime.QuadPart;

    if (fbi.LastWriteTime.QuadPart != 0)
        mtime = (uint64_t)fbi.LastWriteTime.QuadPart;

    if (fbi.ChangeTime.QuadPart != 0)
        ctime = (uint64_t)fbi.ChangeTime.QuadPart;

    // FIXME - FileAttributes

    return nt_status::SUCCESS;
}
