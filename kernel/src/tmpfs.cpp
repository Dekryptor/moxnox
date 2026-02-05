module;

#include <shared_mutex>
#include <span>
#include <optional>
#include <string_view>
#include <mutex>
#include "assert.h"

export module tmpfs;

import nt;
import bsod;
import print;
import mem;
import config;
import list;
import volume;
import timer;
import vfs;
import unicode;
import objects;
import object_ref;
import page_cache;
import spinlock;
import kernel_time_page;
import module_init;

using namespace std;

static const size_t NAME_MAX_LEN = 255;

class tmpfs_filesystem;

class tmpfs_inode : public inode {
public:
    tmpfs_inode(tmpfs_filesystem& fs, enum inode_type inode_type);
    virtual ~tmpfs_inode() override;
    void release() override;

    nt_status open_child(string_view name, object_ref& obj) override;
    nt_status create_child(string_view name, uint32_t create_options,
                           optional<reference_wrapper<inode>>& obj) override;
    NTSTATUS query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    NTSTATUS set_information(span<const uint8_t> buffer, dentry& dentry,
                             FILE_INFORMATION_CLASS file_information_class) override;
    NTSTATUS query_information(span<uint8_t> buffer,
                               FILE_INFORMATION_CLASS file_information_class,
                               uint64_t& retlen) override;
    nt_status query_directory_inode(string_view name, span<uint8_t> buf,
                                    FILE_INFORMATION_CLASS file_information_class) override;
    nt_status rename(::dentry& dentry, ::dentry& dest_dir, string_view name,
                     bool replace_if_exists, object_ref& victim) override;
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages) override;
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status set_basic_information(span<const uint8_t> buffer);
    nt_status query_basic_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_standard_information(span<uint8_t> buffer, uint64_t& retlen);
    void last_handle_closed() override { }
    nt_status set_disposition_information(span<const uint8_t> buffer,
                                          dentry& dentry);

    tmpfs_filesystem& fs; // FIXME - use reference in ::inode
    uint64_t mtime, ctime, otime, atime;
    unsigned int nlink = 1;
    spinlock child_lock;
    list_entry children = { &children, &children };
};

struct tmpfs_dentry {
    list_entry le;
    tmpfs_inode* ino;
    unsigned int index;
    size_t name_len;
    char name[0];
};

class tmpfs_filesystem : public ::filesystem {
public:
    tmpfs_filesystem();

    ~tmpfs_filesystem() override {
        if (root_inode)
            root_inode->release();
    }

    nt_status flush(bool make_readonly) override;

    bool readonly = false;
};

tmpfs_filesystem::tmpfs_filesystem() : filesystem(nullptr) {
    auto tmpfs_root_inode = new tmpfs_inode(*this, inode_type::directory);
    if (!tmpfs_root_inode) {
        print("new failed\n");
        return;
    }

    root_inode = tmpfs_root_inode;

    release(); // root_inode grabs reference

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    tmpfs_root_inode->file_size = 0;
    tmpfs_root_inode->atime = cur_time;
    tmpfs_root_inode->ctime = cur_time;
    tmpfs_root_inode->mtime = cur_time;
    tmpfs_root_inode->otime = cur_time;
}

static ::filesystem* create_tmpfs();

static filesystem_recognizer mod_filesystem_type = {
    {}, "tmpfs", nullptr, create_tmpfs
};

static void init_tmpfs() {
    insert_tail_list(filesystem_types, mod_filesystem_type.le);
}

static module_init module_init_btrfs(init_tmpfs);

static ::filesystem* create_tmpfs() {
    auto fs = new tmpfs_filesystem();
    if (!fs)
        return nullptr;

    return fs;
}

// FIXME - make this global?
static bool validate_name(string_view name) {
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

nt_status tmpfs_inode::create_child(string_view name, uint32_t create_options,
                                    optional<reference_wrapper<inode>>& obj) {
    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    bool is_dir = create_options & FILE_DIRECTORY_FILE;

    auto new_ino = new tmpfs_inode(static_cast<tmpfs_filesystem&>(fs),
                                   is_dir ? inode_type::directory : inode_type::file);
    if (!new_ino)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto d = (tmpfs_dentry*)malloc(offsetof(tmpfs_dentry, name) + name.size());
    if (!d) {
        print("malloc failed\n");
        new_ino->release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    new_ino->file_size = 0;
    new_ino->atime = cur_time;
    new_ino->ctime = cur_time;
    new_ino->mtime = cur_time;
    new_ino->otime = cur_time;

    d->ino = new_ino;
    d->name_len = name.size();
    memcpy(d->name, name.data(), name.size());

    new_ino->acquire();

    {
        lock_guard lock(child_lock);

        if (is_list_empty(children))
            d->index = 0;
        else
            d->index = containing_record<tmpfs_dentry>(children.prev).index + 1;

        insert_tail_list(children, d->le);
    }

    obj = *new_ino;

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::open_child(string_view name, object_ref& obj_ref) {
    if (inode_type != inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    tmpfs_inode* ino = nullptr;

    {
        lock_guard lg(child_lock);

        for (auto le = children.next; le != &children; le = le->next) {
            auto& d = containing_record<tmpfs_dentry>(le);
            auto sv = string_view(d.name, d.name_len);

            if (sv == name) {
                d.ino->acquire();
                ino = d.ino;
                break;
            }
        }
    }

    if (!ino)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    obj_ref = *ino;

    return nt_status::SUCCESS;
}

nt_status tmpfs_filesystem::flush(bool make_readonly) {
    if (make_readonly)
        readonly = true;

    return nt_status::SUCCESS;
}

void tmpfs_inode::release() {
    if (--refcount != 0)
        return;

    delete this;
}

tmpfs_inode::tmpfs_inode(tmpfs_filesystem& fs, enum inode_type inode_type) : inode(fs), fs(fs) {
    this->inode_type = inode_type;
}

tmpfs_inode::~tmpfs_inode() {
    while (true) {
        tmpfs_dentry* d;

        {
            lock_guard lock(child_lock);

            if (is_list_empty(children))
                break;

            d = &containing_record<tmpfs_dentry>(children.next);
            remove_entry_list(d->le);
        }

        d->ino->release();
        free(d);
    }
}

static nt_status query_directory_dirent(string_view name, tmpfs_inode& ino,
                                        span<uint8_t> buf,
                                        FILE_INFORMATION_CLASS file_information_class) {
    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::DirectoryInformation: {
            if (buf.size() < offsetof(FILE_DIRECTORY_INFORMATION, FileName))
                return nt_status::BUFFER_TOO_SMALL;

            auto utf16len = utf8_to_utf16_len(name);
            auto len = offsetof(FILE_DIRECTORY_INFORMATION, FileName) + (utf16len * sizeof(char16_t));

            if (buf.size() < len)
                return nt_status::BUFFER_TOO_SMALL;

            auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf.data();

            {
                shared_lock lock(ino.inode_lock);

                fdi.NextEntryOffset = 0;
                fdi.FileIndex = 0;
                fdi.CreationTime.QuadPart = (int64_t)ino.otime;
                fdi.LastAccessTime.QuadPart = (int64_t)ino.atime;
                fdi.LastWriteTime.QuadPart = (int64_t)ino.mtime;
                fdi.ChangeTime.QuadPart = (int64_t)ino.ctime;
                fdi.EndOfFile.QuadPart = (int64_t)ino.file_size;
                fdi.AllocationSize.QuadPart = 0; // FIXME
                fdi.FileAttributes = ino.inode_type == inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0;
            }

            fdi.FileNameLength = (uint32_t)(utf16len * sizeof(char16_t));
            utf8_to_utf16(span(fdi.FileName, utf16len), name);

            buf = buf.subspan(0, len);

            return nt_status::SUCCESS;
        }

        default:
            print("tmpfs query_directory_dirent: unhandled info class {}\n",
                  file_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

NTSTATUS tmpfs_inode::query_directory(span<uint8_t> buf,
                                      FILE_INFORMATION_CLASS file_information_class,
                                      uint64_t& index)  {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    {
        lock_guard lock(child_lock);

        for (auto le = children.next; le != &children; le = le->next) {
            auto& d = containing_record<tmpfs_dentry>(le);

            if (d.index < index)
                continue;

            // FIXME - what if name changes while we're doing this?

            auto ret = query_directory_dirent(string_view(d.name, d.name_len),
                                              *d.ino, buf,
                                              file_information_class);

            index = d.index + 1;

            return ret;
        }
    }

    return nt_status::NO_MORE_FILES;
}

nt_status tmpfs_inode::query_directory_inode(string_view name, span<uint8_t> buf,
                                             FILE_INFORMATION_CLASS file_information_class) {
    return query_directory_dirent(name, *this, buf, file_information_class);
}

nt_status tmpfs_inode::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                       uint64_t& read_pages) {
    page_cache_entry* ent;

    for (uint64_t i = 0; i < num_pages; i++) {
        lock_guard lg(cache.cache_lock);

        if (i == 0)
            ent = cache.find_page_cache_entry(page_num);
        else
            ent = cache.next_page_cache_entry(*ent);

        assert(ent->tree.key == page_num + i);

        if (!ent->pfn) {
            increase_page_refcount(zero_pfn);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            ent->pfn = zero_pfn;
#pragma GCC diagnostic pop
        }
    }

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::flush_cache(uint64_t, uint64_t) {
    return nt_status::SUCCESS;
}

NTSTATUS tmpfs_inode::set_information(span<const uint8_t> buffer, dentry& dentry,
                                      FILE_INFORMATION_CLASS file_information_class) {
    if (static_cast<tmpfs_filesystem&>(fs).readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::BasicInformation:
            return set_basic_information(buffer);

        case FILE_INFORMATION_CLASS::DispositionInformation:
            return set_disposition_information(buffer, dentry);

        default:
            print("tmpfs_inode::set_information: unhandled file information class {}\n",
                  file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status tmpfs_inode::set_basic_information(span<const uint8_t> buffer) {
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

nt_status tmpfs_inode::query_basic_information(span<uint8_t> buffer,
                                               uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    {
        shared_lock llock(inode_lock);

        fbi.CreationTime.QuadPart = (int64_t)otime;
        fbi.LastAccessTime.QuadPart = (int64_t)atime;
        fbi.LastWriteTime.QuadPart = (int64_t)mtime;
        fbi.ChangeTime.QuadPart = (int64_t)ctime;
        fbi.FileAttributes = inode_type == inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0; // FIXME
    }

    retlen = sizeof(fbi);

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::query_standard_information(span<uint8_t> buffer,
                                                  uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STANDARD_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsi = *(FILE_STANDARD_INFORMATION*)buffer.data();

    {
        shared_lock lock(inode_lock);

        fsi.AllocationSize.QuadPart = 0; // FIXME
        fsi.EndOfFile.QuadPart = (int64_t)file_size;
        fsi.NumberOfLinks = 1; // FIXME - can tmpfs have hardlinks?
        fsi.DeletePending = 0; // FIXME
        fsi.Directory = inode_type == inode_type::directory ? 1 : 0;
    }

    retlen = sizeof(fsi);

    return nt_status::SUCCESS;
}

NTSTATUS tmpfs_inode::query_information(span<uint8_t> buffer,
                                        FILE_INFORMATION_CLASS file_information_class,
                                        uint64_t& retlen) {
    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::BasicInformation:
            return query_basic_information(buffer, retlen);

        case FILE_INFORMATION_CLASS::StandardInformation:
            return query_standard_information(buffer, retlen);

        default:
            print("tmpfs_inode::query_information: unhandled file information class {}\n",
                  file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status tmpfs_inode::set_disposition_information(span<const uint8_t> buffer,
                                                   dentry& dentry) {
    if (buffer.size() < sizeof(FILE_DISPOSITION_INFORMATION))
        return nt_status::INVALID_PARAMETER;

    if (dentry.inode == fs.root_inode)
        return nt_status::INVALID_PARAMETER;

    const auto& fdi = *(FILE_DISPOSITION_INFORMATION*)buffer.data();

    // FIXME - this should actually be a non-POSIX deletion

    if (!fdi.DeleteFile) // FIXME
        return nt_status::NOT_SUPPORTED;

    // FIXME - make sure we can't move or rename file at the same time

    char* name_buf;
    size_t name_len;

    while (true) {
        {
            lock_guard lock(dentry.lock);
            name_len = dentry.name_len;
        }

        assert(name_len != 0);

        name_buf = (char*)malloc(name_len);
        if (!name_buf)
            return nt_status::INSUFFICIENT_RESOURCES;

        bool done = false;

        {
            lock_guard lock(dentry.lock);

            if (dentry.name_len == name_len) {
                memcpy(name_buf, dentry.name, name_len);
                done = true;
            }
        }

        if (done)
            break;

        free(name_buf);
    }

    auto name = string_view(name_buf, name_len);

    {
        auto& p = static_cast<tmpfs_inode&>(*dentry.parent->inode);

        bool done = false;

        {
            lock_guard lock(p.child_lock);

            if (inode_type == inode_type::directory) {
                lock_guard lock(child_lock);

                if (!is_list_empty(children))
                    return nt_status::DIRECTORY_NOT_EMPTY;
            }

            for (auto le = p.children.next; le != &p.children; le = le->next) {
                auto& d = containing_record<tmpfs_dentry>(le);

                if (string_view(d.name, d.name_len) == name) {
                    remove_entry_list(d.le);
                    free(&d);
                    done = true;
                    break;
                }
            }
        }

        if (!done) {
            print("tmpfs_inode::set_disposition_information: tmpfs_dentry not found\n");
            free(name_buf);
            return nt_status::INTERNAL_ERROR;
        }
    }

    free(name_buf);

    // invalidate dentry

    {
        lock_guard lg(dentry.parent->child_lock);
        remove_entry_list(dentry.le);
    }

    {
        lock_guard lg(dentry.lock);
        dentry.status = dentry_status::invalid;
    }

    dentry.release();
    this->release();

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::rename(::dentry& dentry, ::dentry& dest_dir,
                              string_view name, bool replace_if_exists,
                              object_ref& victim) {
    nt_status status;

    assert(dentry.parent);
    assert(dentry.parent->inode);

    if (!validate_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    auto& src_parent_ino = static_cast<tmpfs_inode&>(*dentry.parent->inode);
    auto& dst_parent_ino = static_cast<tmpfs_inode&>(*dest_dir.inode);
    auto cur_name = string_view(dentry.name, dentry.name_len);

    // early return if trying to replace file with itself
    // FIXME - what does NT do here? Does it depend on replace_if_exists?
    if (&src_parent_ino == &dst_parent_ino && name == cur_name)
        return nt_status::SUCCESS;

    // FIXME - lock inode to prevent simultaneous renames?

    bool done_overwrite = false;
    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    // FIXME - overwrites

    status = dst_parent_ino.open_child(name, victim);
    if (!success(status) && status != nt_status::OBJECT_NAME_NOT_FOUND) {
        print("tmpfs_inode::open_child returned {}\n", status);
        return status;
    }

    if (success(status)) {
        if (!replace_if_exists)
            return nt_status::OBJECT_NAME_COLLISION;

        assert(victim.get().type == object_type::inode);

        auto& victim_ino = static_cast<tmpfs_inode&>(victim.get());

        if (&victim_ino == fs.root_inode)
            return nt_status::ACCESS_DENIED;

        {
            lock_guard lg2(victim_ino.inode_lock);

            if (victim_ino.inode_type == inode_type::directory) {
                if (inode_type != inode_type::directory)
                    return nt_status::ACCESS_DENIED;

                if (victim_ino.file_size != 0)
                    return nt_status::DIRECTORY_NOT_EMPTY;
            } else {
                if (inode_type == inode_type::directory)
                    return nt_status::ACCESS_DENIED;
            }

            victim_ino.nlink--;
            victim_ino.ctime = cur_time;

            // pretend we have an open handle
            victim_ino.handle_count++;
        }

        {
            bool done = false;

            {
                lock_guard lock(dst_parent_ino.child_lock);

                if (victim_ino.inode_type == inode_type::directory) {
                    lock_guard lock(victim_ino.child_lock);

                    if (!is_list_empty(victim_ino.children))
                        return nt_status::DIRECTORY_NOT_EMPTY;
                }

                for (auto le = dst_parent_ino.children.next; le != &dst_parent_ino.children; le = le->next) {
                    auto& d2 = containing_record<tmpfs_dentry>(le);

                    if (string_view(d2.name, d2.name_len) == name) {
                        remove_entry_list(d2.le);
                        free(&d2);
                        done = true;
                        break;
                    }
                }
            }

            if (!done) {
                print("tmpfs_inode::rename: victim tmpfs_dentry not found\n");
                return nt_status::INTERNAL_ERROR;
            }

            victim_ino.release();
        }

        done_overwrite = true;
    }

    // add new tmpfs dentry

    auto d = (tmpfs_dentry*)malloc(offsetof(tmpfs_dentry, name) + name.size());
    if (!d) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    d->ino = this;
    d->name_len = name.size();
    memcpy(d->name, name.data(), name.size());

    acquire();

    {
        lock_guard lock(dst_parent_ino.child_lock);

        if (is_list_empty(dst_parent_ino.children))
            d->index = 0;
        else
            d->index = containing_record<tmpfs_dentry>(dst_parent_ino.children.prev).index + 1;

        insert_tail_list(dst_parent_ino.children, d->le);
    }

    // remove old tmpfs dentry

    {
        bool done = false;

        {
            lock_guard lock(src_parent_ino.child_lock);

            if (inode_type == inode_type::directory) {
                lock_guard lock(child_lock);

                if (!is_list_empty(children))
                    return nt_status::DIRECTORY_NOT_EMPTY;
            }

            for (auto le = src_parent_ino.children.next; le != &src_parent_ino.children; le = le->next) {
                auto& d2 = containing_record<tmpfs_dentry>(le);

                if (string_view(d2.name, d2.name_len) == cur_name) {
                    remove_entry_list(d2.le);
                    free(&d2);
                    done = true;
                    break;
                }
            }
        }

        if (!done) {
            print("tmpfs_inode::rename: existing tmpfs_dentry not found\n");
            return nt_status::INTERNAL_ERROR;
        }

        release();
    }

    // update inode

    {
        lock_guard lg(inode_lock);

        ctime = cur_time;
    }

    // adjust size etc. of parent directories

    {
        lock_guard lg(src_parent_ino.inode_lock);

        // FIXME - size munging needs to be done in same lock as checking victim_ino size?

        if (&src_parent_ino == &dst_parent_ino && !done_overwrite)
            src_parent_ino.file_size += name.size() * 2;

        src_parent_ino.file_size -= cur_name.size() * 2;
        src_parent_ino.ctime = cur_time;
        src_parent_ino.mtime = cur_time;
    }

    if (&src_parent_ino != &dst_parent_ino) {
        lock_guard lg(dst_parent_ino.inode_lock);

        if (!done_overwrite)
            dst_parent_ino.file_size += name.size() * 2;

        dst_parent_ino.ctime = cur_time;
        dst_parent_ino.mtime = cur_time;
    }

    return nt_status::SUCCESS;
}
