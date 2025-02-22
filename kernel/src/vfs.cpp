#include "kernel.h"
#include "spinlock.h"
#include "objects.h"
#include "volume.h"
#include "handle.h"

import format;

using namespace std;

static const guid moxnox_partition_guid = { 0x5dd71063, 0x95c9, 0x41b2, { 0x82, 0x52, 0x84, 0x13, 0xfe, 0xac, 0x64, 0x90 } };

static spinlock fs_lock;
static list_entry fs_list = { &fs_list, &fs_list };
static filesystem* root_fs = nullptr;

void add_fs_to_list(filesystem& fs) {
    lock_guard lock(fs_lock);
    insert_tail_list(fs_list, fs.le);
}

bool mount_root() {
    string_view cmdline_root;

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

    if (!cmdline_root.empty()) {
        static const char prefix[] = "/dev/";

        if (cmdline_root.size() <= sizeof(prefix) - 1 ||
            string_view(cmdline_root.data(), sizeof(prefix) - 1) != prefix) {
            bsod_format("malformed root option {}", cmdline_root);
        }

        cmdline_root = string_view(cmdline_root.data() + sizeof(prefix) - 1,
                                   cmdline_root.size() - sizeof(prefix) + 1);

        auto ref = open_object(cmdline_root);
        if (!ref)
            bsod_format("could not find root volume /dev/{}", cmdline_root);

        if (ref.get().type != object_type::volume) {
            bsod_format("root volume /dev/{} is not a block device",
                        cmdline_root);
        }

        auto& vol = static_cast<volume&>(ref.get());

        {
            lock_guard lock(vol.mount_lock);

            if (!vol.filesystem) {
                bsod_format("no recognized filesystem found on /dev/{}",
                            cmdline_root);
            }

            root_fs = &vol.filesystem->get();
            root_fs->refcount++;

            return true;
        }
    }

    // search for MN partition if no option

    lock_guard lock(fs_lock);

    if (is_list_empty(fs_list)) {
        print("No filesytems found.\n");
        return false;
    }

    // choose MN partition

    auto le = fs_list.next;
    while (le != &fs_list) {
        auto& fs = containing_record<filesystem>(le);

        if (fs.vol.type_guid == moxnox_partition_guid) {
            if (root_fs) {
                print("Multiple Moxnox partitions, not sure which to choose\n");
                root_fs = nullptr;
                return false;
            }

            root_fs = &fs;
        }

        le = le->next;
    }

    if (!root_fs) {
        print("No Moxnox partition found\n");
        return false;
    }

    return true;
}

nt_status open_inode(string_view fn, const optional<reference_wrapper<inode>>& dir,
                     object_ref& obj_ref, uint32_t create_disposition,
                     uint32_t create_options) {
    if (dir && fn.empty()) {
        dir->get().acquire();
        obj_ref = dir->get();
        return nt_status::SUCCESS;
    }

    if (!root_fs) {
        print("No root filesystem.\n");
        return nt_status::INTERNAL_ERROR;
    }

    // FIXME - handle mount points
    // FIXME - /dev

    return root_fs->open(fn, dir, obj_ref, create_disposition,
                         create_options);
}


NTSTATUS NtReadFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                    IO_STATUS_BLOCK* io_status_block, void* buffer,
                    uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t*) {
    auto buf = span((uint8_t*)buffer, length);

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - async (event)
    // FIXME - async (APC)

    auto& obj = objrw->get();
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

    if (!can_read) {
        visit_obj(obj, [&](auto&& o) {
            o.release();
        });

        return nt_status::INVALID_PARAMETER;
    }

    NTSTATUS status;
    size_t read;

    if (byte_offset) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
                status = o.read(buf, byte_offset->QuadPart, read);

            o.release();
        });
    } else {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
                status = o.read(buf, read);

            o.release();
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

NTSTATUS NtCreateFile(HANDLE* file_handle, ACCESS_MASK,
                      const OBJECT_ATTRIBUTES* object_attributes, IO_STATUS_BLOCK* io_status_block,
                      const LARGE_INTEGER*, uint32_t, uint32_t, uint32_t create_disposition,
                      uint32_t create_options, void*, uint32_t) {
    NTSTATUS status;
    auto t = get_current_thread();
    auto p = t->proc;
    object_ref root_dir;

    if (!p)
        return nt_status::INVALID_PARAMETER;

    if (create_options & FILE_NON_DIRECTORY_FILE && create_options & FILE_DIRECTORY_FILE)
        return nt_status::INVALID_PARAMETER;

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

    if (namelen > dev_prefix.size() && string_view(name, dev_prefix.size()) == dev_prefix) {
        if (create_disposition != FILE_OPEN)
            return nt_status::NOT_SUPPORTED;

        ino_ref = open_object(string_view(name + dev_prefix.size(), namelen - dev_prefix.size()));
    } else {
        if (root_dir) {
            status = open_inode(string_view(name, namelen), static_cast<inode&>(static_cast<file&>(root_dir.get()).ino),
                                ino_ref, create_disposition, create_options);
        } else {
            status = open_inode(string_view(name, namelen), nullopt, ino_ref,
                                create_disposition, create_options);
        }

        if (!success(status)) {
            // FIXME - distinguish between OBJECT_NAME_NOT_FOUND and OBJECT_PATH_NOT_FOUND, etc.
            free(name);
            return status;
        }
    }

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

    auto h = p->add_handle(*f, 0); // FIXME - access mask

    if (!h) {
        delete f;
        return nt_status::INTERNAL_ERROR;
    }

    *file_handle = h;
    io_status_block->Information = FILE_OPENED;

    return nt_status::SUCCESS;
}

NTSTATUS NtClose(HANDLE handle) {
    auto t = get_current_thread();
    auto p = t->proc;

    if (!p)
        return nt_status::INVALID_PARAMETER;

    return p->close_handle(handle);
}

NTSTATUS NtWriteFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                     IO_STATUS_BLOCK* io_status_block, const void* buffer,
                     uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t*) {
    auto buf = span((const uint8_t*)buffer, length);

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

    if (byte_offset) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_writable_object_with_offset<remove_reference_t<decltype(o)>>)
                written = o.write(buf, byte_offset->QuadPart);

            o.release();
        });
    } else {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
                written = o.write(buf);

            o.release();
        });
    }

    // FIXME - propagating errors from write function

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = written;

    return nt_status::SUCCESS;
}

nt_status flush_filesystems() {
    // FIXME - loop through mounts

    if (!root_fs)
        return nt_status::SUCCESS;

    return root_fs->flush();
}
