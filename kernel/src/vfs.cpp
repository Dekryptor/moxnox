module;

#include <shared_mutex>
#include "kernel.h"
#include "spinlock.h"
import objects;
import rbtree;
#include "inode.h"
#include "handle.h"
import rbtree;
import event;
import pipe;
import timer;
import volume;
#include "visit_object.h"

export module vfs;

import print;
import bsod;
import tmpfs;
import cpio;
import mem;
import config;
import list;
import string;
import rwsem;

using namespace std;

static const guid moxnox_partition_guid = { 0x5dd71063, 0x95c9, 0x41b2, { 0x82, 0x52, 0x84, 0x13, 0xfe, 0xac, 0x64, 0x90 } };

struct mount {
    mount(size_t path_len) : path_len(path_len) { }

    ~mount() {
        if (fs->refcount.fetch_sub(1) == 1)
            delete fs;
    }

    list_entry le;
    ::filesystem* fs = nullptr;
    spinlock children_lock;
    list_entry children = { &children, &children };
    size_t path_len;
    char path[0];
};

static rwsem fs_lock;
static list_entry fs_list = { &fs_list, &fs_list };
static mount root(0);

export struct filesystem_recognizer {
    list_entry le;
    bool (*func)(volume&);
};

export list_entry filesystem_types = { &filesystem_types, &filesystem_types };

export void add_fs_to_list(::filesystem& fs) {
    lock_guard lock(fs_lock);
    insert_tail_list(fs_list, fs.le);
}

static ::filesystem* do_mount_root(string_view cmdline_root) {
    ::filesystem* ret = nullptr;

    if (!cmdline_root.empty()) {
        static const char prefix[] = "/dev/";

        if (cmdline_root.size() <= sizeof(prefix) - 1 ||
            string_view(cmdline_root.data(), sizeof(prefix) - 1) != prefix) {
            bsod_format("malformed root option {}", cmdline_root);
        }

        cmdline_root = string_view(cmdline_root.data() + sizeof(prefix) - 1,
                                   cmdline_root.size() - sizeof(prefix) + 1);

        object_ref ref;

        auto status = open_object(cmdline_root, ref);
        if (!success(status))
            bsod_format("Error {} while trying to open root volume /dev/{}", status, cmdline_root);

        if (ref.get().type != object_type::volume) {
            bsod_format("root volume /dev/{} is not a block device",
                        cmdline_root);
        }

        auto& vol = static_cast<volume&>(ref.get());

        {
            lock_guard lock(vol.mount_lock);

            if (!vol.filesystem) {
                for (auto le = filesystem_types.next; le != &filesystem_types; le = le->next) {
                    auto& r = containing_record<filesystem_recognizer>(le);

                    if (r.func(vol))
                        break;
                }
            }

            if (!vol.filesystem) {
                bsod_format("no recognized filesystem found on /dev/{}",
                            cmdline_root);
            }

            ret = &vol.filesystem->get();
            ret->refcount++;

            return ret;
        }
    }

    // search for MN partition if no option

    lock_guard lock(fs_lock);

    if (is_list_empty(fs_list)) {
        print("No filesytems found.\n");
        return nullptr;
    }

    // choose MN partition

    auto le = fs_list.next;
    while (le != &fs_list) {
        auto& fs = containing_record<::filesystem>(le);

        if (fs.vol && fs.vol->type_guid == moxnox_partition_guid) {
            if (ret) {
                print("Multiple Moxnox partitions, not sure which to choose\n");
                return nullptr;
            }

            ret = &fs;
        }

        le = le->next;
    }

    if (!ret) {
        print("No Moxnox partition found\n");
        return nullptr;
    }

    return ret;
}

static const uint64_t __S_IFMT = 0170000;
static const uint64_t __S_IFDIR = 0040000;
static const uint64_t __S_IFREG = 0100000;

static constexpr bool S_ISDIR(uint64_t mode) {
    return (mode & __S_IFMT) == __S_IFDIR;
}

static constexpr bool S_ISREG(uint64_t mode) {
    return (mode & __S_IFMT) == __S_IFREG;
}

static uint64_t unix_to_system_time(uint64_t t) {
    return (t * 10000000) + 116444736000000000;
}

static nt_status extract_initrd_file(tmpfs_filesystem& fs, string_view path,
                                     uint64_t mode, span<const uint8_t> contents,
                                     uint64_t mtime) {
    // FIXME - support symlinks as well at least
    if (!S_ISDIR(mode) && !S_ISREG(mode))
        return nt_status::SUCCESS;

    // ignore root directory
    if (S_ISDIR(mode) && path == ".")
        return nt_status::SUCCESS;

    object_ref file;
    unsigned int create_result;

    auto status = fs.open(path, *fs.root_inode, file, FILE_CREATE,
                          S_ISDIR(mode) ? FILE_DIRECTORY_FILE : 0,
                          create_result);

    if (!success(status))
        return status;

    auto& ino = static_cast<tmpfs_inode&>(file.get());

    ino.mtime = unix_to_system_time(mtime);

    if (!S_ISREG(mode))
        return nt_status::SUCCESS;

    uint64_t offset = 0, written;

    while (!contents.empty()) {
        status = ino.write(contents, offset, written);
        if (!success(status))
            return status;

        contents = span(contents.data() + written, contents.size() - written);
        offset += written;
    }

    return nt_status::SUCCESS;
}

static bool extract_initrd() {
    auto fs = create_tmpfs();
    if (!fs) {
        print("create_tmpfs failed\n");
        return false;
    }

    root.fs = fs;

    process_cpio(initrd, [&](string_view path, uint64_t mode, span<const uint8_t> contents, uint64_t mtime) {
        auto status = extract_initrd_file(*fs, path, mode, contents, mtime);

        if (!success(status))
            bsod_format("extract_initrd_file returned {} for {}", status, path);
    });

    auto num_pages = bytes_to_pages(initrd.size());

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_and_free_page((uintptr_t)(initrd.data() + (i << PAGE_SHIFT)));
    }

    initrd = span<uint8_t>();

    return true;
}

export bool mount_root() {
    string_view cmdline_root;

    if (!initrd.empty())
        return extract_initrd();

    loop_through_cmdline([&](string_view s) {
        static const char root_option[] = "root=";

        if (s.size() < sizeof(root_option) - 1)
            return true;

        if (string_view(s.data(), sizeof(root_option) - 1) != root_option)
            return true;

        cmdline_root = string_view(s.data() + sizeof(root_option) - 1,
                                   s.size() - sizeof(root_option) + 1);

        return false;
    });

    auto root_fs = do_mount_root(cmdline_root);
    if (!root_fs)
        return false;

    root.fs = root_fs;

    return true;
}

export nt_status open_inode(string_view fn, const optional<reference_wrapper<inode>>& dir,
                            object_ref& obj_ref, uint32_t create_disposition,
                            uint32_t create_options, unsigned int& create_result) {
    if (dir && fn.empty()) {
        dir->get().acquire();
        obj_ref = dir->get();
        return nt_status::SUCCESS;
    }

    if (!root.fs) {
        print("No root filesystem.\n");
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - handle mount points
    // FIXME - /dev

    return root.fs->open(fn, dir, obj_ref, create_disposition,
                         create_options, create_result);
}

export extern "C"
NTSTATUS NtReadFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                    IO_STATUS_BLOCK* io_status_block, void* buffer,
                    uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t*) {
    auto buf = span((uint8_t*)buffer, length);

    if (byte_offset && byte_offset->QuadPart < 0)
        return nt_status::INVALID_PARAMETER;

    auto objrw = deref_handle2(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - async (event)
    // FIXME - async (APC)

    auto& obj = objrw.get();
    bool can_read = false;

    if (byte_offset) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
                can_read = true;
        });
    } else {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
                can_read = true;
        });
    }

    if (!can_read)
        return nt_status::INVALID_PARAMETER;

    NTSTATUS status;
    size_t read;

    if (byte_offset) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
                status = o.read(buf, (uint64_t)byte_offset->QuadPart, read);
        });
    } else {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
                status = o.read(buf, read);
        });
    }

    if (!success(status)) {
        io_status_block->Status = status;
        io_status_block->Information = 0;
        return status;
    }

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = read;

    return nt_status::SUCCESS;
}

export extern "C"
NTSTATUS NtCreateFile(HANDLE* file_handle, ACCESS_MASK,
                      const OBJECT_ATTRIBUTES* object_attributes, IO_STATUS_BLOCK* io_status_block,
                      const LARGE_INTEGER*, uint32_t, uint32_t, uint32_t create_disposition,
                      uint32_t create_options, void*, uint32_t) {
    NTSTATUS status;
    auto t = get_current_thread();
    auto p = t->proc;
    object_ref root_dir;
    bool shared_okay = true;

    if (!p)
        return nt_status::INVALID_PARAMETER;

    if (create_options & FILE_NON_DIRECTORY_FILE && create_options & FILE_DIRECTORY_FILE)
        return nt_status::INVALID_PARAMETER;

    switch (create_disposition) {
        case FILE_SUPERSEDE:
        case FILE_OPEN:
        case FILE_CREATE:
        case FILE_OPEN_IF:
        case FILE_OVERWRITE:
        case FILE_OVERWRITE_IF:
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (!object_attributes->ObjectName) // FIXME - is this allowed if RootDirectory is set?
        return nt_status::INVALID_PARAMETER;

    if (object_attributes->RootDirectory) {
        bool valid = false;

        root_dir = deref_handle2(object_attributes->RootDirectory);

        if (!root_dir)
            return nt_status::INVALID_HANDLE;

        visit_obj(root_dir.get(), [&](auto&& o) {
            if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>) {
                if (o.ino.type == object_type::inode)
                    valid = true;
            }
        });

        if (!valid)
            return nt_status::INVALID_HANDLE;
    }

    // FIXME - check permissions (whether valid, and against SD)
    // FIXME - creating files
    // FIXME - allocation_size
    // FIXME - file_attributes
    // FIXME - share_access
    // FIXME - EAs

    auto u16name = u16string_view(object_attributes->ObjectName->Buffer, object_attributes->ObjectName->Length / sizeof(char16_t));

    if (u16name.empty())
        return nt_status::INVALID_PARAMETER;

    auto namelen = utf16_to_utf8_len(u16name);

    if (namelen == 0)
        return nt_status::INVALID_PARAMETER;

    auto name = (char*)malloc(namelen);
    if (!name)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(name, namelen), u16name);

    for (unsigned int i = 0; i < namelen; i++) {
        if (name[i] == '\\')
            name[i] = '/';
    }

    // FIXME - proper VFS system

    static const string_view dev_prefix = "/dev/";
    object_ref ino_ref;
    unsigned int create_result;

    if (namelen > dev_prefix.size() && string_view(name, dev_prefix.size()) == dev_prefix) {
        if (create_disposition != FILE_OPEN)
            return nt_status::NOT_SUPPORTED;

        status = open_object(string_view(name + dev_prefix.size(), namelen - dev_prefix.size()),
                             ino_ref);
        if (!success(status)) {
            free(name);
            return status;
        }

        create_result = FILE_OPENED;
    } else {
        if (root_dir) {
            status = open_inode(string_view(name, namelen), static_cast<inode&>(static_cast<file&>(root_dir.get()).ino),
                                ino_ref, create_disposition, create_options, create_result);
        } else {
            status = open_inode(string_view(name, namelen), nullopt, ino_ref,
                                create_disposition, create_options, create_result);
        }

        if (!success(status)) {
            // FIXME - distinguish between OBJECT_NAME_NOT_FOUND and OBJECT_PATH_NOT_FOUND, etc.
            free(name);
            return status;
        }
    }

    visit_obj(ino_ref.get(), [&](auto&& o) {
        if constexpr (has_exclusive_field<remove_reference_t<decltype(o)>>)
            shared_okay = !o.exclusive;
    });

    handle_t h;

    if (ino_ref.get().type == object_type::inode) {
        const auto& ino = static_cast<inode&>(ino_ref.get());

        if (create_options & FILE_NON_DIRECTORY_FILE && ino.inode_type == inode_type::directory)
            return nt_status::FILE_IS_A_DIRECTORY;
        else if (create_options & FILE_DIRECTORY_FILE && ino.inode_type != inode_type::directory)
            return nt_status::NOT_A_DIRECTORY;

        auto f = new file(ino_ref.get());

        if (!f) {
            free(name);
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        f->name = name;
        f->namelen = namelen;

        status = p->add_handle(*f, 0, shared_okay, h); // FIXME - access mask
        if (!success(status)) {
            delete f;
            return status;
        }

        f->release();
    } else {
        status = p->add_handle(ino_ref.get(), 0, shared_okay, h); // FIXME - access mask
        if (!success(status)) {
            visit_obj(ino_ref.get(), [&](auto&& o) {
                o.release();
            });
            return status;
        }
    }

    *file_handle = h;

    io_status_block->Information = create_result;

    return nt_status::SUCCESS;
}

export extern "C"
NTSTATUS NtClose(HANDLE handle) {
    auto t = get_current_thread();
    auto p = t->proc;

    if (!p)
        return nt_status::INVALID_PARAMETER;

    return p->close_handle(handle);
}

export extern "C"
NTSTATUS NtWriteFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                     IO_STATUS_BLOCK* io_status_block, const void* buffer,
                     uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t*) {
    auto buf = span((const uint8_t*)buffer, length);

    if (byte_offset && byte_offset->QuadPart < 0)
        return nt_status::INVALID_PARAMETER;

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - async (event)
    // FIXME - async (APC)

    auto& obj = objrw->get();
    bool can_write = false;

    if (byte_offset) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_writable_object_with_offset<remove_reference_t<decltype(o)>>)
                can_write = true;
        });
    } else {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
                can_write = true;
        });
    }

    if (!can_write) {
        visit_obj(obj, [&](auto&& o) {
            o.release();
        });

        return nt_status::INVALID_PARAMETER;
    }

    size_t written;
    nt_status status;

    if (byte_offset) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_writable_object_with_offset<remove_reference_t<decltype(o)>>)
                status = o.write(buf, (uint64_t)byte_offset->QuadPart, written);

            o.release();
        });
    } else {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
                status = o.write(buf, written);

            o.release();
        });
    }

    if (!success(status)) {
        io_status_block->Status = status;
        io_status_block->Information = 0;
        return status;
    }

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = written;

    return nt_status::SUCCESS;
}

export nt_status flush_filesystems(bool make_readonly) {
    lock_guard lock(fs_lock);

    // FIXME - what should we do if there's a failure - carry on?

    for (auto le = fs_list.next; le != &fs_list; le = le->next) {
        auto& fs = containing_record<::filesystem>(le);

        auto status = fs.flush(make_readonly);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

extern "C++"
nt_status inode::read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
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

extern "C++"
nt_status inode::write(span<const uint8_t> s, uint64_t offset, uint64_t& written_bytes) {
    // FIXME - don't allow if FS readonly

    if (s.empty()) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    // FIXME - should have lock for this
    if (file_size < offset + s.size())
        file_size = offset + s.size();

    written_bytes = cache.write(s, offset);

    // FIXME - mark inode as dirty

    return nt_status::SUCCESS;
}
