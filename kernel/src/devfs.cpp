module;

#include <stdint.h>
#include <string_view>
#include <optional>
#include <span>
#include <mutex>
#include <shared_mutex>

export module devfs;

import nt;
import vfs;
import list;
import print;
import kernel_time_page;
import spinlock;
import object_ref;
import handle;
import objects;
import unicode;
import module_init;

using namespace std;

class devfs_filesystem : public ::filesystem {
public:
    devfs_filesystem();

    ~devfs_filesystem() override {
        if (root_inode)
            root_inode->release();
    }

    nt_status flush(bool make_readonly) override;

    spinlock inode_lock;
    list_entry inode_list = { &inode_list, &inode_list };
};

class devfs_inode : public inode {
public:
    devfs_inode(devfs_filesystem& fs, enum inode_type inode_type);
    void release() override;

    nt_status open_child(string_view name, object_ref& obj) override;
    nt_status create_child(string_view name, uint32_t create_options,
                           optional<reference_wrapper<inode>>& obj) override;
    nt_status query_directory(span<uint8_t> buf, FILE_INFORMATION_CLASS file_information_class,
                             uint64_t& index) override;
    nt_status query_directory_inode(string_view name, span<uint8_t> buf,
                                    FILE_INFORMATION_CLASS file_information_class) override;
    nt_status set_information(span<const uint8_t> buffer, dentry& dentry,
                              FILE_INFORMATION_CLASS file_information_class) override;
    nt_status query_information(span<uint8_t> buffer,
                                FILE_INFORMATION_CLASS file_information_class,
                                uint64_t& retlen) override;
    nt_status rename(::dentry& dentry, ::dentry& dest_dir, string_view name,
                     bool replace_if_exists, object_ref& victim) override;
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages) override;
    nt_status flush_cache(uint64_t page_num, uint64_t num_pages) override;
    nt_status query_basic_information(span<uint8_t> buffer, uint64_t& retlen);
    nt_status query_standard_information(span<uint8_t> buffer, uint64_t& retlen);
    void last_handle_closed() override { }

    devfs_filesystem& fs; // FIXME - use reference in ::inode
    list_entry le;
    uint64_t mtime, ctime, otime, atime;
};

static ::filesystem* create_devfs();

static filesystem_recognizer mod_filesystem_type = {
    {}, "devfs", nullptr, create_devfs
};

static void init_devfs() {
    insert_tail_list(filesystem_types, mod_filesystem_type.le);
}

static module_init module_init_btrfs(init_devfs);

static ::filesystem* create_devfs() {
    auto fs = new devfs_filesystem;
    if (!fs)
        return nullptr;

    return fs;
}

devfs_filesystem::devfs_filesystem() : filesystem(nullptr) {
    auto devfs_root_inode = new devfs_inode(*this, inode_type::directory);
    if (!devfs_root_inode) {
        print("new failed\n");
        return;
    }

    root_inode = devfs_root_inode;

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    devfs_root_inode->atime = cur_time;
    devfs_root_inode->ctime = cur_time;
    devfs_root_inode->mtime = cur_time;
    devfs_root_inode->otime = cur_time;

    {
        lock_guard lg(inode_lock);
        insert_tail_list(inode_list, devfs_root_inode->le);
    }
}

nt_status devfs_filesystem::flush(bool) {
    return nt_status::SUCCESS;
}

devfs_inode::devfs_inode(devfs_filesystem& fs, enum inode_type inode_type) : inode(fs), fs(fs) {
    this->inode_type = inode_type;
}

void devfs_inode::release() {
    {
        lock_guard lock(static_cast<devfs_filesystem&>(fs).inode_lock);

        if (--refcount != 0)
            return;

        remove_entry_list(le);
    }

    delete this;
}

nt_status devfs_inode::open_child(string_view name, object_ref& obj) {
    // FIXME - allow child directories
    if (this != fs.root_inode)
        return nt_status::OBJECT_NAME_NOT_FOUND;

    return open_object(name, obj);
}

nt_status devfs_inode::create_child(string_view /*name*/, uint32_t /*create_options*/,
                                    optional<reference_wrapper<inode>>& /*obj*/) {
    print("FIXME - devfs_inode::create_child\n");

    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}

static nt_status query_directory_dirent(string_view name, bool is_dir,
                                        uint64_t time, span<uint8_t> buf,
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

            fdi.NextEntryOffset = 0;
            fdi.FileIndex = 0;
            fdi.CreationTime.QuadPart = (int64_t)time;
            fdi.LastAccessTime.QuadPart = (int64_t)time;
            fdi.LastWriteTime.QuadPart = (int64_t)time;
            fdi.ChangeTime.QuadPart = (int64_t)time;
            fdi.EndOfFile.QuadPart = 0;
            fdi.AllocationSize.QuadPart = 0;
            fdi.FileAttributes = is_dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_DEVICE;

            fdi.FileNameLength = (uint32_t)(utf16len * sizeof(char16_t));
            utf8_to_utf16(span(fdi.FileName, utf16len), name);

            buf = buf.subspan(0, len);

            return nt_status::SUCCESS;
        }

        default:
            print("devfs query_directory_dirent: unhandled info class {}\n",
                  file_information_class);
            return nt_status::INVALID_INFO_CLASS;
    }
}

nt_status devfs_inode::query_directory(span<uint8_t> buf,
                                       FILE_INFORMATION_CLASS file_information_class,
                                       uint64_t& index) {
    if (inode_type != inode_type::directory)
        return nt_status::NOT_A_DIRECTORY;

    object_ref obj;
    uint64_t time;
    char name_buf[MAX_NAME_LEN];
    string_view name;

    {
        lock_guard lock(name_lock);

        for (auto le = name_list.next; le != &name_list; le = le->next) {
            auto& ne = containing_record<name_entry>(le);

            if (ne.index < index)
                continue;

            time = ne.creation_time;
            index = ne.index + 1;
            memcpy(name_buf, ne.name, ne.name_len);
            name = string_view(name_buf, ne.name_len);

            break;
        }
    }

    if (name.empty())
        return nt_status::NO_MORE_FILES;

    return query_directory_dirent(name, false, time, buf, file_information_class);
}

nt_status devfs_inode::query_directory_inode(string_view name, span<uint8_t> buf,
                                             FILE_INFORMATION_CLASS file_information_class) {
    // FIXME - store creation time and size for directories

    return query_directory_dirent(name, inode_type == inode_type::directory,
                                  0, buf, file_information_class);
}

nt_status devfs_inode::set_information(span<const uint8_t> /*buffer*/,
                                       dentry& /*dentry*/,
                                       FILE_INFORMATION_CLASS /*file_information_class*/) {
    print("FIXME - devfs_inode::set_information\n");

    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}

nt_status devfs_inode::query_basic_information(span<uint8_t> buffer,
                                               uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fbi = *(FILE_BASIC_INFORMATION*)buffer.data();

    {
        shared_lock lg(inode_lock);

        fbi.CreationTime.QuadPart = (int64_t)otime;
        fbi.LastAccessTime.QuadPart = (int64_t)atime;
        fbi.LastWriteTime.QuadPart = (int64_t)mtime;
        fbi.ChangeTime.QuadPart = (int64_t)ctime;
        fbi.FileAttributes = inode_type == inode_type::directory ? FILE_ATTRIBUTE_DIRECTORY : 0; // FIXME
    }

    retlen = sizeof(fbi);

    return nt_status::SUCCESS;
}

nt_status devfs_inode::query_standard_information(span<uint8_t> buffer,
                                                  uint64_t& retlen) {
    if (buffer.size() < sizeof(FILE_STANDARD_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& fsi = *(FILE_STANDARD_INFORMATION*)buffer.data();

    {
        shared_lock lock(inode_lock);

        fsi.AllocationSize.QuadPart = 0; // FIXME
        fsi.EndOfFile.QuadPart = (int64_t)file_size;
        fsi.NumberOfLinks = 1;
        fsi.DeletePending = 0; // FIXME
        fsi.Directory = inode_type == inode_type::directory ? 1 : 0;
    }

    retlen = sizeof(fsi);

    return nt_status::SUCCESS;
}

nt_status devfs_inode::query_information(span<uint8_t> buffer,
                                         FILE_INFORMATION_CLASS file_information_class,
                                         uint64_t& retlen) {
    switch (file_information_class) {
        case FILE_INFORMATION_CLASS::BasicInformation:
            return query_basic_information(buffer, retlen);

        case FILE_INFORMATION_CLASS::StandardInformation:
            return query_standard_information(buffer, retlen);

        default:
            print("devfs_inode::query_information: unhandled file information class {}\n",
                 (unsigned int)file_information_class);
            return nt_status::NOT_IMPLEMENTED;
    }
}

nt_status devfs_inode::read_into_cache(uint64_t /*page_num*/, uint64_t /*num_pages*/,
                                       uint64_t& /*read_pages*/) {
    return nt_status::NOT_SUPPORTED;
}

nt_status devfs_inode::flush_cache(uint64_t /*page_num*/, uint64_t /*num_pages*/) {
    return nt_status::NOT_SUPPORTED;
}

nt_status devfs_inode::rename(::dentry& /*dentry*/, ::dentry& /*dest_dir*/,
                              string_view /*name*/, bool /*replace_if_exists*/,
                              object_ref& /*victim*/) {
    return nt_status::NOT_SUPPORTED;
}
