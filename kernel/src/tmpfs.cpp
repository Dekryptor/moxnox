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
import object_ref;
import page_cache;
import spinlock;
import kernel_time_page;

using namespace std;

static const size_t NAME_MAX_LEN = 255;

class tmpfs_filesystem;

class tmpfs_inode : public inode {
public:
    tmpfs_inode(tmpfs_filesystem& fs, enum inode_type inode_type);
    void release() override;

    nt_status open_child(string_view name,
                         optional<reference_wrapper<inode>>& obj) override;
    nt_status create_child(string_view name, uint32_t create_options,
                           optional<reference_wrapper<inode>>& obj) override;
    NTSTATUS query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    NTSTATUS set_information(span<const uint8_t> buffer,
                             FILE_INFORMATION_CLASS file_information_class) override;
    NTSTATUS query_information(span<uint8_t> buffer,
                               FILE_INFORMATION_CLASS file_information_class,
                               uint64_t& retlen) override;
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages) override;
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status set_basic_information(span<const uint8_t> buffer);
    nt_status query_standard_information(span<uint8_t> buffer, uint64_t& retlen);

    tmpfs_filesystem& fs; // FIXME - use reference in ::inode
    list_entry le;
    uint64_t mtime, ctime, otime, atime;
};

class tmpfs_filesystem : public ::filesystem {
public:
    tmpfs_filesystem();

    ~tmpfs_filesystem() override {
        if (root_inode)
            root_inode->release();
    }

    nt_status flush(bool make_readonly) override;

    spinlock inode_lock;
    list_entry inode_list = { &inode_list, &inode_list };
    bool readonly = false;
};

tmpfs_filesystem::tmpfs_filesystem() : filesystem(nullptr) {
    auto tmpfs_root_inode = new tmpfs_inode(*this, inode_type::directory);
    if (!tmpfs_root_inode) {
        print("new failed\n");
        return;
    }

    root_inode = tmpfs_root_inode;

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    tmpfs_root_inode->atime = cur_time;
    tmpfs_root_inode->ctime = cur_time;
    tmpfs_root_inode->mtime = cur_time;
    tmpfs_root_inode->otime = cur_time;

    insert_tail_list(inode_list, tmpfs_root_inode->le);
}

extern "C++" ::filesystem* create_tmpfs() {
    auto fs = new tmpfs_filesystem();
    if (!fs)
        return nullptr;

    fs->refcount++;

    return fs;
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

nt_status tmpfs_inode::create_child(string_view name, uint32_t create_options,
                                    optional<reference_wrapper<inode>>& obj) {
    if (fs.readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    if (!invalid_name(name))
        return nt_status::OBJECT_NAME_INVALID;

    bool is_dir = create_options & FILE_DIRECTORY_FILE;

    auto new_ino = new tmpfs_inode(static_cast<tmpfs_filesystem&>(fs),
                                   is_dir ? inode_type::directory : inode_type::file);
    if (!new_ino)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    new_ino->file_size = 0;
    new_ino->atime = cur_time;
    new_ino->ctime = cur_time;
    new_ino->mtime = cur_time;
    new_ino->otime = cur_time;

    obj = *new_ino;

    return nt_status::SUCCESS;
}

nt_status tmpfs_inode::open_child(string_view,
                                  optional<reference_wrapper<inode>>&) {
    if (inode_type != inode_type::directory)
        return nt_status::INVALID_PARAMETER;

    // tmpfs doesn't clear the dentry cache, so if it's not there it doesn't exist

    return nt_status::OBJECT_NAME_NOT_FOUND;
}

nt_status tmpfs_filesystem::flush(bool make_readonly) {
    if (make_readonly)
        readonly = true;

    return nt_status::SUCCESS;
}

void tmpfs_inode::release() {
    {
        lock_guard lock(static_cast<tmpfs_filesystem&>(fs).inode_lock);

        if (--refcount != 0)
            return;

        remove_entry_list(le);
    }

    delete this;
}

tmpfs_inode::tmpfs_inode(tmpfs_filesystem& fs, enum inode_type inode_type) : inode(fs), fs(fs) {
    this->inode_type = inode_type;
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

            auto& ino = *static_cast<tmpfs_inode*>(d.inode);

            {
                shared_lock lock(d.inode->inode_lock);

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

    auto& d = *dentry;

    {
        shared_lock lock(d.child_lock);

        for (auto le = d.children.next; le != &d.children; le = le->next) {
            auto& c = containing_record<::dentry>(le);

            if (c.index < index)
                continue;

            auto ret = query_directory_dirent(c, buf, file_information_class);

            index = d.index + 1;

            return ret;
        }
    }

    return nt_status::NO_MORE_FILES;
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

NTSTATUS tmpfs_inode::set_information(span<const uint8_t> buffer,
                                      FILE_INFORMATION_CLASS file_information_class) {
    if (static_cast<tmpfs_filesystem&>(fs).readonly)
        return nt_status::MEDIA_WRITE_PROTECTED;

    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::BasicInformation:
            return set_basic_information(buffer);

        default:
            print("tmpfs_inode::set_information: unhandled file information class {}\n",
                  (unsigned int)file_information_class);
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
        case FILE_INFORMATION_CLASS::StandardInformation:
            return query_standard_information(buffer, retlen);

        default:
            print("tmpfs_inode::query_information: unhandled file information class {}\n",
                 (unsigned int)file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}
