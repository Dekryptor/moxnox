#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "handle.h"

using namespace std;

struct name_entry {
    list_entry le;
    object* obj;
    size_t name_len;
    char name[1];
};

static spinlock name_lock;
static list_entry name_list = { &name_list, &name_list };

optional<reference_wrapper<object>> deref_handle(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle(h);
}

object_ref deref_handle2(handle_t h) {
    auto t = get_current_thread();

    if (!t->proc)
        return nullopt;

    return t->proc->deref_handle2(h);
}

bool name_object(object& obj, string_view name, bool persistent) {
    if (name.empty()) {
        // FIXME - exception
        printk("Invalid name\n");
        return false;
    }

    auto ne = (name_entry*)malloc(offsetof(name_entry, name) + name.size());
    if (!ne) {
        // FIXME - exception
        printk("Out of memory\n");
        return false;
    }

    optional<lock_guard<spinlock>> lock(name_lock);

    if (obj.named) {
        lock.reset();
        // FIXME - exception
        printk("Object already has name\n");
        free(ne);
        return false;
    }

    auto le = name_list.next;
    while (le != &name_list) {
        auto& ne2 = containing_record<name_entry>(le);

        if (ne2.name_len == name.length()) {
            auto ne2sv = string_view(ne2.name, ne2.name_len);

            if (ne2sv == name) {
                lock.reset();
                // FIXME - exception
                printk("Name collision\n");
                free(ne);
                return false;
            }
        }

        le = le->next;
    }

    obj.named = true;
    obj.persistent = persistent;
    ne->obj = &obj;
    ne->name_len = name.length();
    memcpy(ne->name, name.data(), name.length());
    insert_tail_list(name_list, ne->le);

    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });

    return true;
}

bool name_object(handle_t h, string_view name, bool persistent) {
    auto objrw = deref_handle(h);
    if (!objrw) {
        // FIXME - exception
        printk("No such handle\n");
        return false;
    }

    auto& obj = objrw->get();

    auto ret = name_object(obj, name, persistent);

    visit_obj(obj, [](auto&& o) {
        o.release();
    });

    return ret;
}

object_ref open_object(string_view name) {
    if (name.empty()) {
        printk("Invalid name\n");
        return {};
    }

    object* obj = nullptr;

    {
        lock_guard lock(name_lock);

        auto le = name_list.next;
        while (le != &name_list) {
            auto& ne = containing_record<name_entry>(le);

            if (ne.name_len == name.length()) {
                auto nesv = string_view(ne.name, ne.name_len);

                if (nesv == name) {
                    visit_obj(*ne.obj, [](auto&& o) {
                        o.acquire();
                    });

                    obj = ne.obj;
                    break;
                }
            }

            le = le->next;
        }
    }

    if (!obj) {
        printk("Not found\n");
        return {};
    }

    return *obj;
}

void write(object_ref& h, span<const uint8_t> s) {
    if (!h) {
        // FIXME - exception
        printk("Object not set\n");
        return;
    }

    auto& obj = h.get();
    bool can_write = false;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            can_write = true;
    });

    if (!can_write) {
        // FIXME - exception
        printk("Not a writable object\n");
        return;
    }

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            o.write(s);
    });
}

void write(object_ref& h, string_view s) {
    write(h, span((uint8_t*)s.data(), s.size()));
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

void handle_closed(object& obj) {
    auto new_handle_count = --obj.handle_count;

    // remove name if last handle closed

    if (obj.named && !obj.persistent && new_handle_count == 0) {
        name_entry* ne = nullptr;

        {
            lock_guard lock(name_lock);

            auto le = name_list.next;
            while (le != &name_list) {
                auto& ne2 = containing_record<name_entry>(le);

                if (ne2.obj == &obj) {
                    ne = &ne2;
                    remove_entry_list(ne2.le);
                    break;
                }

                le = le->next;
            }
        }

        if (ne) {
            free(ne);

            visit_obj(obj, [](auto&& o) {
                o.release();
            });
        }
    }
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

    if (create_disposition != FILE_OPEN) {
        printk("FIXME - creating files\n");
        return nt_status::NOT_SUPPORTED;
    }

    // FIXME - check permissions (whether valid, and against SD)
    // FIXME - create_options
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

    if (namelen > dev_prefix.size() && string_view(name, dev_prefix.size()) == dev_prefix)
        ino_ref = open_object(string_view(name + dev_prefix.size(), namelen - dev_prefix.size()));
    else {
        if (root_dir)
            ino_ref = open_inode(string_view(name, namelen), static_cast<inode&>(static_cast<file&>(root_dir.get()).ino));
        else
            ino_ref = open_inode(string_view(name, namelen), nullopt);
    }

    if (!ino_ref) {
        // FIXME - distinguish between OBJECT_NAME_NOT_FOUND and OBJECT_PATH_NOT_FOUND, etc.
        free(name);
        return nt_status::OBJECT_NAME_NOT_FOUND;
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
