#include "kernel.h"
#include "objects.h"
#include "spinlock.h"
#include "unique_handle.h"

using namespace std;

static unsigned int no_handles = 0;
static handle_obj* const handle_arr = (handle_obj*)0xfffffa2000000000;
static size_t handle_arr_pages = 0;
static spinlock handle_spinlock;

struct name_entry {
    list_entry le;
    object* obj;
    size_t name_len;
    char name[1];
};

static spinlock name_lock;
static list_entry name_list = { &name_list, &name_list };

handle_t add_kernel_handle(object& obj, uint32_t access_mask) {
    handle_t h;

    // FIXME - reusing handles?

    {
        lock_guard lock(handle_spinlock);

        size_t needed_pages = bytes_to_pages((no_handles + 1) * sizeof(handle_obj));

        if (needed_pages > handle_arr_pages) {
            auto pa = allocate_page();
            if (!pa) {
                // FIXME - should be exception
                print("allocate_page failed\n");
                return 0;
            }

            if (!map_page(pa, (uintptr_t)handle_arr + (handle_arr_pages << PAGE_SHIFT), mapping_type::read_write)) {
                // FIXME - exception
                print("map_page failed\n");
                free_page(pa);
                return 0;
            }

            handle_arr_pages++;
        }

        no_handles++;

        h = kernel_handle_mask | no_handles;

        auto& ho = handle_arr[h - 1];
        ho.obj = &obj;
        ho.access_mask = access_mask;
    }

    visit_obj(obj, [](auto&& o) {
        o.acquire();
    });

    obj.handle_count++;

    return h;
}

void close_handle(handle_t h) {
    object* obj;

    {
        lock_guard lock(handle_spinlock);

        auto& ho = handle_arr[(h & ~kernel_handle_mask) - 1];
        obj = ho.obj;

        ho.obj = nullptr;
    }

    handle_closed(*obj);

    visit_obj(*obj, [](auto&& o) {
        o.release();
    });
}

optional<reference_wrapper<object>> deref_handle(handle_t h) {
    if (!(h & kernel_handle_mask)) {
        auto t = get_current_thread();

        if (!t->proc)
            return nullopt;

        return t->proc->deref_handle(h);
    }

    lock_guard lock(handle_spinlock);

    if (h == 0 || (h & ~kernel_handle_mask) > no_handles)
        return nullopt;

    auto& ho = handle_arr[(h & ~kernel_handle_mask) - 1];

    if (!ho.obj)
        return nullopt;

    visit_obj(*ho.obj, [](auto&& o) {
        o.acquire(); // needs to be within spinlock
    });

    return *ho.obj;
}

bool name_object(object& obj, string_view name, bool persistent) {
    if (name.empty()) {
        // FIXME - exception
        print("Invalid name\n");
        return false;
    }

    auto ne = (name_entry*)malloc(offsetof(name_entry, name) + name.size());
    if (!ne) {
        // FIXME - exception
        print("Out of memory\n");
        return false;
    }

    optional<lock_guard<spinlock>> lock(name_lock);

    if (obj.named) {
        lock.reset();
        // FIXME - exception
        print("Object already has name\n");
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
                print("Name collision\n");
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
        print("No such handle\n");
        return false;
    }

    auto& obj = objrw->get();

    auto ret = name_object(obj, name, persistent);

    visit_obj(obj, [](auto&& o) {
        o.release();
    });

    return ret;
}

unique_handle open_object(string_view name) {
    if (name.empty()) {
        print("Invalid name\n");
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
        print("Not found\n");
        return {};
    }

    auto hv = add_kernel_handle(*obj, 0); // FIXME - access mask

    visit_obj(*obj, [](auto&& o) {
        o.release();
    });

    if (!hv) {
        print("add_kernel_handle failed\n");
        return {};
    }

    return unique_handle{hv};
}

size_t read(handle_t h, span<uint8_t> s, optional<size_t> offset) {
    auto objrw = deref_handle(h);

    if (!objrw) {
        // FIXME - exception
        print("Invalid handle\n");
        return 0;
    }

    auto& obj = objrw->get();
    bool can_read = false;

    visit_obj(obj, [&](auto&& o) {
        if (offset) {
            if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
                can_read = true;
        } else {
            if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
                can_read = true;
        }
    });

    if (!can_read) {
        // FIXME - exception
        print("Can't read from object\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return 0;
    }

    size_t ret;

    visit_obj(obj, [&](auto&& o) {
        if (offset) {
            if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
                ret = o.read(s, *offset);
            else
                ret = 0; // can't happen - this is to make compiler happy
        } else {
            if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
                ret = o.read(s);
            else
                ret = 0;
        }

        o.release();
    });

    return ret;
}

void write(handle_t h, span<const uint8_t> s) {
    auto objrw = deref_handle(h);

    if (!objrw) {
        // FIXME - exception
        print("Invalid handle\n");
        return;
    }

    auto& obj = objrw->get();
    bool can_write = false;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            can_write = true;
    });

    if (!can_write) {
        // FIXME - exception
        print("Not a writable object\n");

        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return;
    }

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            o.write(s);

        o.release();
    });
}

void write(handle_t h, string_view s) {
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

    size_t read;

    if (byte_offset) {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
                read = o.read(buf, byte_offset->QuadPart);

            o.release();
        });
    } else {
        visit_obj(obj, [&](auto&& o) {
            if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
                read = o.read(buf);

            o.release();
        });
    }

    // FIXME - propagating errors from reada function

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = read;

    return nt_status::SUCCESS;
}

NTSTATUS NtCreateFile(HANDLE* file_handle, ACCESS_MASK,
                      const OBJECT_ATTRIBUTES* object_attributes, IO_STATUS_BLOCK* io_status_block,
                      const LARGE_INTEGER*, uint32_t, uint32_t, uint32_t create_disposition,
                      uint32_t, void*, uint32_t) {
    auto t = get_current_thread();
    auto p = t->proc;

    if (!p) // FIXME - allow creating kernel handles?
        return nt_status::INVALID_PARAMETER;

    // FIXME - object_attributes->RootDirectory

    if (!object_attributes->ObjectName)
        return nt_status::INVALID_PARAMETER;

    if (create_disposition != FILE_OPEN) {
        print("FIXME - creating files\n");
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

    auto inoh = open_file(string_view(name, namelen));

    free(name);

    if (!inoh) {
        // FIXME - distinguish between OBJECT_NAME_NOT_FOUND and OBJECT_PATH_NOT_FOUND, etc.
        return nt_status::OBJECT_NAME_NOT_FOUND;
    }

    auto objrw = deref_handle(inoh.get());

    if (!objrw)
        return nt_status::INTERNAL_ERROR;

    auto& ino = static_cast<inode&>(objrw->get());

    auto f = new file(ino);
    if (!f) {
        ino.release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ino.release();

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
    if (handle & kernel_handle_mask) {
        close_handle(handle);
        return nt_status::SUCCESS;
    }

    auto t = get_current_thread();
    auto p = t->proc;

    if (!p)
        return nt_status::INVALID_PARAMETER;

    return p->close_handle(handle);
}
