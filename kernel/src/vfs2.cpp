module;

#include <shared_mutex>
#include <span>
#include <optional>
#include <string_view>
#include <mutex>
#include <stdint.h>

module vfs;

import print;
import bsod;
import cpio;
import config;
import unicode;
import guid;
import cmdline;
import thread;
import visit;
import handle;
import crt;

using namespace std;

// tmpfs.cpp (FIXME)
extern "C++" ::filesystem* create_tmpfs();

// devfs.cpp (FIXME)
extern "C++" ::filesystem* create_devfs();

static const guid moxnox_partition_guid = { 0x5dd71063, 0x95c9, 0x41b2, { 0x82, 0x52, 0x84, 0x13, 0xfe, 0xac, 0x64, 0x90 } };

static rwsem fs_lock;
static list_entry fs_list = { &fs_list, &fs_list };

void add_fs_to_list(::filesystem& fs) {
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

    for (auto le = fs_list.next; le != &fs_list; le = le->next) {
        auto& fs = containing_record<::filesystem>(le);

        if (fs.vol && fs.vol->type_guid == moxnox_partition_guid) {
            if (ret) {
                print("Multiple Moxnox partitions, not sure which to choose\n");
                return nullptr;
            }

            ret = &fs;
        }
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

static nt_status extract_initrd_file(string_view path, uint64_t mode,
                                     span<const uint8_t> contents,
                                     uint64_t mtime) {
    // FIXME - support symlinks as well at least
    if (!S_ISDIR(mode) && !S_ISREG(mode))
        return nt_status::SUCCESS;

    // ignore root directory
    if (S_ISDIR(mode) && path == ".")
        return nt_status::SUCCESS;

    object_ref inorw;
    optional<reference_wrapper<dentry>> drw;
    unsigned int create_result;

    auto status = open_inode(path, *root_dentry, inorw, drw, FILE_CREATE,
                             S_ISDIR(mode) ? FILE_DIRECTORY_FILE : 0,
                             create_result);
    if (!success(status))
        return status;

    auto& ino = static_cast<inode&>(inorw.get());
    FILE_BASIC_INFORMATION fbi;

    memset(&fbi, 0, sizeof(fbi));

    fbi.LastWriteTime.QuadPart = (int64_t)unix_to_system_time(mtime);

    status = ino.set_information(span((const uint8_t*)&fbi, sizeof(fbi)), drw->get(),
                                 FILE_INFORMATION_CLASS::BasicInformation);

    if (drw)
        drw->get().release();


    if (!success(status))
        return status;

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

    root_dentry = new dentry;
    if (!root_dentry) {
        print("new failed\n");
        return false;
    }

    root_dentry->inode = fs->root_inode;
    root_dentry->name_len = 0;
    root_dentry->parent = nullptr;

    root_dentry->inode->acquire();

    process_cpio(initrd, [&](string_view path, uint64_t mode, span<const uint8_t> contents, uint64_t mtime) {
        auto status = extract_initrd_file(path, mode, contents, mtime);

        if (!success(status))
            bsod_format("extract_initrd_file returned {} for {}", status, path);
    });

    auto num_pages = bytes_to_pages(initrd.size());

    for (unsigned int i = 0; i < num_pages; i++) {
        auto va = (uintptr_t)(initrd.data() + (i << PAGE_SHIFT));
        auto pa = va_to_pa_lookup(va);

        unmap_page(va);
        free_folio(pa);
    }

    initrd = span<uint8_t>();

    return true;
}

bool mount_root() {
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

    root_dentry = new dentry;
    if (!root_dentry) {
        print("new failed\n");
        return false;
    }

    root_dentry->inode = root_fs->root_inode;
    root_dentry->name_len = 0;
    root_dentry->parent = nullptr;

    root_dentry->inode->acquire();

    return true;
}

bool mount_devfs() {
    object_ref dev;
    optional<reference_wrapper<dentry>> drw;
    unsigned int result;

    // FIXME - do this in init instead

    auto status = open_inode("/dev", *root_dentry, dev, drw, FILE_OPEN_IF,
                             FILE_DIRECTORY_FILE, result);
    if (!success(status)) {
        print("open_inode returned {}\n", status);
        return false;
    }

    if (!drw) {
        print("open_inode did not return a dentry\n");
        return false;
    }

    auto& d = drw->get();

    if (dev.get().type != object_type::inode) {
        print("/dev was {}, expected inode\n", dev.get().type);
        d.release();
        return false;
    }

    auto& ino = static_cast<inode&>(dev.get());

    if (ino.inode_type != inode_type::directory) {
        print("/dev was {}, expected directory\n", ino.inode_type);
        d.release();
        return false;
    }

    auto fs = create_devfs();
    if (!fs) {
        print("create_devfs failed\n");
        return false;
    }

    inode* ino2 = fs->root_inode;

    ino2->acquire();

    {
        lock_guard lg(d.parent->child_lock);
        swap(d.inode, ino2);
    }

    ino2->release();

    return true;
}

static bool add_to_dentry_cache(dentry& dir, string_view name, inode& ino,
                                optional<reference_wrapper<dentry>>& drw) {
    auto p = malloc(sizeof(dentry));
    if (!p)
        return false;

    auto& d = *new (p) dentry;

    d.name_len = name.size();

    if (!name.empty()) {
        d.name = (char*)malloc(name.size());
        if (!d.name) {
            d.~dentry();
            free(p);
            return false;
        }

        memcpy(d.name, name.data(), name.size());
    }

    d.inode = &ino;
    ino.acquire();

    insert_tail_list(dir.children, d.le);

    d.parent = &dir;
    dir.acquire();

    drw = d;

    return true;
}

static void search_dentry_cache(dentry& dir, string_view name,
                                optional<reference_wrapper<dentry>>& drw) {
    for (auto le = dir.children.next; le != &dir.children; le = le->next) {
        auto& d = containing_record<dentry>(le);
        auto sv = string_view(d.name, d.name_len);

        if (sv == name) {
            drw = d;
            d.acquire();
            return;
        }
    }
};

nt_status open_inode(string_view fn, dentry& root_dir,
                     object_ref& obj_ref, optional<reference_wrapper<dentry>>& dentry,
                     uint32_t create_disposition, uint32_t create_options,
                     unsigned int& create_result) {
    while (!fn.empty() && fn.front() == '/') {
        fn = string_view(fn.data() + 1, fn.size() - 1);
    }

    reference_wrapper<::dentry> dirrw{root_dir};
    dirrw.get().acquire();

    // FIXME - die if anything in middle not directory

    while (true) {
        auto sl = fn.find("/");
        auto& dir = dirrw.get();
        bool last_bit = sl == string_view::npos;
        optional<reference_wrapper<::dentry>> drw;
        auto name = last_bit ? fn : string_view(fn.data(), sl);
        auto dispo = last_bit ? create_disposition : FILE_OPEN;
        nt_status status;
        object_ref obj_ref2;

        // FIXME - mount points
        // FIXME - negative dentries?
        // FIXME - dentry LRU

        // FIXME - FILE_SUPERSEDE, FILE_OVERWRITE, FILE_OVERWRITE_IF

        if (name.empty() || name == ".") {
            if (dir.inode->inode_type != inode_type::directory) {
                dir.release();
                return nt_status::OBJECT_PATH_SYNTAX_BAD;
            }

            drw = dir;
            dir.acquire();
        } else if (name == "..") {
            if (dir.inode->inode_type != inode_type::directory) {
                dir.release();
                return nt_status::OBJECT_PATH_SYNTAX_BAD;
            }

            ::dentry* parent;

            {
                lock_guard lg(dir.lock);

                if (!dir.parent)
                    parent = &dir;
                else
                    parent = dir.parent;
            }

            parent->acquire();
            dir.release();

            drw = *parent;
        } else {
            shared_lock lg(dir.child_lock);

            search_dentry_cache(dir, name, drw);
        }

        if (drw.has_value()) {
            if (dispo == FILE_CREATE) {
                dir.release();
                drw->get().release();
                return nt_status::OBJECT_NAME_COLLISION;
            } else if (last_bit)
                create_result = FILE_OPENED;
        }

        if (!drw.has_value()) {
            lock_guard lg(dir.child_lock);

            search_dentry_cache(dir, name, drw);

            if (drw.has_value()) {
                if (dispo == FILE_CREATE) {
                    dir.release();
                    drw->get().release();
                    return nt_status::OBJECT_NAME_COLLISION;
                } else if (last_bit)
                    create_result = FILE_OPENED;
            }

            if (!drw.has_value()) {
                status = dir.inode->open_child(name, obj_ref2);

                if (success(status)) {
                    if (dispo == FILE_CREATE) {
                        dir.release();
                        return nt_status::OBJECT_NAME_COLLISION;
                    } else if (last_bit)
                        create_result = FILE_OPENED;
                }

                if (status == nt_status::OBJECT_NAME_NOT_FOUND &&
                    (dispo == FILE_CREATE || dispo == FILE_OPEN_IF)) {
                    optional<reference_wrapper<inode>> inorw;

                    status = dir.inode->create_child(name, create_options,
                                                     inorw);
                    if (success(status)) {
                        create_result = FILE_CREATED;

                        obj_ref2 = inorw->get();
                    }
                }

                if (success(status)) {
                    // FIXME - should non-inodes also go in dentry cache?

                    if (obj_ref2.get().type == object_type::inode) {
                        auto& ino = static_cast<inode&>(obj_ref2.get());

                        if (!add_to_dentry_cache(dir, name, ino, drw)) {
                            dir.release();
                            return nt_status::INSUFFICIENT_RESOURCES;
                        }
                    }
                }
            }
        }

        if (!drw.has_value()) {
            dir.release();

            // non-inodes don't get put in dentry cache
            if (last_bit && success(status)) {
                swap(obj_ref, obj_ref2);
                return status;
            }

            if (!last_bit && status == nt_status::OBJECT_NAME_NOT_FOUND)
                status = nt_status::OBJECT_PATH_NOT_FOUND;

            return status;
        }

        auto& d = drw->get();

        dir.release();

        if (last_bit) {
            d.inode->acquire();
            obj_ref = *d.inode;
            dentry = d;

            return nt_status::SUCCESS;
        }

        fn = string_view(fn.data() + sl + 1, fn.size() - sl - 1);
        dirrw = d;
    }
}

extern "C"
NTSTATUS NtReadFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                    IO_STATUS_BLOCK* io_status_block, void* buffer,
                    uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t*) {
    auto buf = span((uint8_t*)buffer, length);

    if (byte_offset && byte_offset->QuadPart < 0)
        return nt_status::INVALID_PARAMETER;

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - async (event)
    // FIXME - async (APC)

    auto& obj = objrw.get();

    NTSTATUS status;
    size_t read;

    if (byte_offset) {
        status = object_read_with_offset(obj, buf, (uint64_t)byte_offset->QuadPart,
                                         read);
    } else
        status = object_read_without_offset(obj, buf, read);

    if (!success(status)) {
        io_status_block->Status = status;
        io_status_block->Information = 0;
        return status;
    }

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = read;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtCreateFile(HANDLE* file_handle, ACCESS_MASK access_mask,
                      const OBJECT_ATTRIBUTES* object_attributes, IO_STATUS_BLOCK* io_status_block,
                      const LARGE_INTEGER*, uint32_t, uint32_t, uint32_t create_disposition,
                      uint32_t create_options, void*, uint32_t) {
    NTSTATUS status;
    auto t = get_current_thread();
    auto p = t->proc;
    optional<reference_wrapper<dentry>> root_dir;
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
        auto root_dir_file = deref_handle(object_attributes->RootDirectory);

        if (!root_dir_file)
            return nt_status::INVALID_HANDLE;

        if (root_dir_file.get().type != object_type::file)
            return nt_status::INVALID_HANDLE;

        auto& f = static_cast<file&>(root_dir_file.get());

        if (f.ino.type != object_type::inode)
            return nt_status::INVALID_HANDLE;

        if (!f.dentry) // shouldn't happen
            return nt_status::INTERNAL_ERROR;

        root_dir = *f.dentry;
    } else
        root_dir = *root_dentry;

    auto& rd = root_dir->get();

    rd.acquire();

    // FIXME - check permissions (whether valid, and against SD)
    // FIXME - allocation_size
    // FIXME - file_attributes
    // FIXME - share_access
    // FIXME - EAs

    auto u16name = u16string_view(object_attributes->ObjectName->Buffer, object_attributes->ObjectName->Length / sizeof(char16_t));

    if (u16name.empty()) {
        rd.release();
        return nt_status::INVALID_PARAMETER;
    }

    auto namelen = utf16_to_utf8_len(u16name);

    if (namelen == 0) {
        rd.release();
        return nt_status::INVALID_PARAMETER;
    }

    auto name = (char*)malloc(namelen);
    if (!name) {
        rd.release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    utf16_to_utf8(span(name, namelen), u16name);

    for (unsigned int i = 0; i < namelen; i++) {
        if (name[i] == '\\')
            name[i] = '/';
    }

    object_ref ino_ref;
    optional<reference_wrapper<dentry>> drw;
    unsigned int create_result;

    status = open_inode(string_view(name, namelen), rd,
                        ino_ref, drw, create_disposition, create_options,
                        create_result);
    if (!success(status)) {
        free(name);
        rd.release();
        return status;
    }

    free(name);
    rd.release();

    shared_okay = !object_is_exclusive(ino_ref.get());

    handle_t h;

    if (drw)
        drw->get().release();

    // FIXME - should non-inodes also be encapsulated in file?

    if (ino_ref.get().type == object_type::inode) {
        const auto& ino = static_cast<inode&>(ino_ref.get());

        if (create_options & FILE_NON_DIRECTORY_FILE && ino.inode_type == inode_type::directory)
            return nt_status::FILE_IS_A_DIRECTORY;
        else if (create_options & FILE_DIRECTORY_FILE && ino.inode_type != inode_type::directory)
            return nt_status::NOT_A_DIRECTORY;

        auto f = new file(ino_ref.get(), create_options);

        if (!f)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (drw)
            f->dentry = &drw->get();

        status = p->add_handle(*f, access_mask, shared_okay, h);
        if (!success(status)) {
            delete f;
            return status;
        }

        if (f->dentry)
            f->dentry->open_file_count++;

        f->release();
    } else {
        if (drw) // FIXME - shouldn't happen, as only inodes get dentries at the moment
            drw->get().release();

        status = p->add_handle(ino_ref.get(), access_mask, shared_okay, h);
        if (!success(status))
            return status;
    }

    *file_handle = h;

    io_status_block->Information = create_result;

    return nt_status::SUCCESS;
}

extern "C"
NTSTATUS NtOpenFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                    const OBJECT_ATTRIBUTES* object_attributes,
                    IO_STATUS_BLOCK* iosb, uint32_t share_access,
                    uint32_t open_options) {
    return NtCreateFile(file_handle, desired_access, object_attributes, iosb,
                        nullptr, 0, share_access, FILE_OPEN, open_options,
                        nullptr, 0);
}

extern "C"
NTSTATUS NtWriteFile(HANDLE file_handle, HANDLE, PIO_APC_ROUTINE, void*,
                     IO_STATUS_BLOCK* io_status_block, const void* buffer,
                     uint32_t length, const LARGE_INTEGER* byte_offset, const uint32_t*) {
    ACCESS_MASK access_mask;

    static const uint64_t WRITE_TO_END_OF_FILE = 0xffffffffffffffff;

    auto buf = span((const uint8_t*)buffer, length);

    auto objrw = deref_handle(file_handle, access_mask);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    if (!(access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)))
        return nt_status::ACCESS_DENIED;

    // FIXME - async (event)
    // FIXME - async (APC)

    auto& obj = objrw.get();
    optional<uint64_t> offset;

    if (byte_offset)
        offset = (uint64_t)byte_offset->QuadPart;

    if ((access_mask & FILE_APPEND_DATA) && !(access_mask & FILE_WRITE_DATA))
        offset = WRITE_TO_END_OF_FILE;

    size_t written;
    nt_status status;

    if (offset.has_value() && *offset == WRITE_TO_END_OF_FILE) {
        if (obj.type != object_type::file)
            status = nt_status::INVALID_PARAMETER;
        else {
            auto& f = static_cast<file&>(obj);

            if (f.ino.type != object_type::inode)
                status = nt_status::INVALID_PARAMETER;
            else {
                auto& ino = static_cast<inode&>(f.ino);

                {
                    lock_guard lock(ino.inode_lock);
                    offset = ino.file_size;
                }

                status = f.write(buf, *offset, written);
            }
        }
    } else if (offset.has_value())
        status = object_write_with_offset(obj, buf, *offset, written);
    else
        status = object_write_without_offset(obj, buf, written);

    if (!success(status)) {
        io_status_block->Status = status;
        io_status_block->Information = 0;
        return status;
    }

    io_status_block->Status = nt_status::SUCCESS;
    io_status_block->Information = written;

    return nt_status::SUCCESS;
}

nt_status flush_filesystems(bool make_readonly) {
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

nt_status inode::read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    uint64_t size;

    {
        shared_lock lock(inode_lock);
        size = file_size;
    }

    if (offset >= size)
        return nt_status::END_OF_FILE;

    if (s.empty()) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    if (offset + s.size() > size)
        s = s.subspan(0, size - offset);

    auto status = cache.read(s, offset);
    if (!success(status))
        return status;

    read_bytes = s.size();
    return nt_status::SUCCESS;
}

nt_status inode::write(span<const uint8_t> s, uint64_t offset, uint64_t& written_bytes) {
    // FIXME - don't allow if FS readonly

    if (s.empty()) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    {
        lock_guard lock(inode_lock);

        if (file_size < offset + s.size())
            file_size = offset + s.size();
    }

    written_bytes = cache.write(s, offset);

    return nt_status::SUCCESS;
}

file::file(object& ino, uint32_t create_options) : object(object_type::file), ino(ino), create_options(create_options) {
    object_acquire(ino);
}

file::~file() {
    object_release(ino);

    if (dentry)
        dentry->release();
}

NTSTATUS file::read(span<uint8_t> s, uint64_t& read_bytes) {
    NTSTATUS status;

    status = object_read_without_offset(ino, s, read_bytes);
    if (status != nt_status::NOT_SUPPORTED)
        return status;

    status = object_read_with_offset(ino, s, file_offset, read_bytes);

    if (success(status)) {
        lock_guard l(lock);
        file_offset += read_bytes;
    }

    return status;
}

NTSTATUS file::read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    auto status = object_read_with_offset(ino, s, offset, read_bytes);

    if (success(status)) {
        lock_guard l(lock);
        file_offset = offset + read_bytes;
    }

    return status;
}

NTSTATUS file::write(span<const uint8_t> s, uint64_t& written_bytes) {
    NTSTATUS status;

    // FIXME - update file position if FILE_SYNCHRONOUS_IO_ALERT or FILE_SYNCHRONOUS_IO_NONALERT set

    status = object_write_without_offset(ino, s, written_bytes);
    if (status != nt_status::NOT_SUPPORTED)
        return status;

    status = object_write_with_offset(ino, s, file_offset, written_bytes);

    if (success(status) && written_bytes > 0) {
        lock_guard l(lock);
        file_offset += written_bytes;
    }

    return status;
}

NTSTATUS file::write(span<const uint8_t> s, uint64_t offset, uint64_t& written_bytes) {
    // FIXME - if FILE_SYNCHRONOUS_IO_ALERT or FILE_SYNCHRONOUS_IO_NONALERT set, set file_offset to offset first

    auto status = object_write_with_offset(ino, s, offset, written_bytes);

    if (success(status) && written_bytes > 0) {
        lock_guard l(lock);
        file_offset = offset + written_bytes;
    }

    return status;
}

extern "C"
NTSTATUS NtQueryDirectoryFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                              void* apc_context, IO_STATUS_BLOCK* io_status_block,
                              void* file_information, uint32_t length,
                              FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                              const UNICODE_STRING* file_name, bool restart_scan) {
    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_directory(event, apc_routine, apc_context, *io_status_block, file_information,
                                 length, file_information_class, return_single_entry,
                                 file_name, restart_scan);

    return ret;
}

NTSTATUS file::query_directory(HANDLE, PIO_APC_ROUTINE, void*, IO_STATUS_BLOCK& io_status_block,
                               void* file_information, uint32_t length,
                               FILE_INFORMATION_CLASS file_information_class, bool,
                               const UNICODE_STRING*, bool restart_scan) {
    nt_status status;

    // FIXME - event
    // FIXME - APC
    // FIXME - filename

    if (restart_scan)
        query_dir_index = 0;

    // FIXME - returning multiple entries

    auto index = query_dir_index;
    auto sp = span((uint8_t*)file_information, length);

    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    if (index == 0) {
        status = static_cast<inode&>(ino).query_directory_inode(".", sp,
                                                                file_information_class);
        index = 1;
    } else {
        status = static_cast<inode&>(ino).query_directory(sp, file_information_class, index);
        if (!success(status))
            return status;
    }

    query_dir_index = index;
    io_status_block.Information = sp.size();

    return status;
}

extern "C"
NTSTATUS NtSetInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer,
                              uint32_t buffer_size, FILE_INFORMATION_CLASS file_information_class) {
    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    iosb->Information = 0;

    return f.set_information(buffer, buffer_size, file_information_class);
}

NTSTATUS file::set_information(void* buffer, uint32_t buffer_size,
                               FILE_INFORMATION_CLASS file_information_class) {
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    return static_cast<inode&>(ino).set_information(span((uint8_t*)buffer, buffer_size),
                                                    *dentry,  file_information_class);
}

extern "C"
NTSTATUS NtQueryInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer,
                                uint32_t buffer_size, FILE_INFORMATION_CLASS file_information_class) {
    uint64_t retlen;

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_information(buffer, buffer_size, file_information_class,
                                   retlen);

    if (!success(ret)) {
        iosb->Information = 0;
        return ret;
    }

    iosb->Information = retlen;

    return ret;
}

NTSTATUS file::query_information(void* buffer, uint32_t buffer_size,
                                 FILE_INFORMATION_CLASS file_information_class,
                                 uint64_t& retlen) {
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    return static_cast<inode&>(ino).query_information(span((uint8_t*)buffer, buffer_size),
                                                      file_information_class, retlen);
}

nt_status get_dentry_path(dentry& dr, char*& path, size_t& path_len) {
    size_t len;
    char* s;

    while (true) {
        size_t left;
        bool done = false;

        // go to root and count characters

        len = 0;

        auto* d = &dr;
        while (d->name_len != 0) {
            {
                lock_guard lg(d->lock);
                len += d->name_len + 1;
            }

            d = d->parent;
        }

        s = (char*)malloc(len);
        if (!s)
            return nt_status::INSUFFICIENT_RESOURCES;

        auto p = s + len;

        // go to root again and copy

        left = len;
        d = &dr;
        while (d->name_len != 0) {
            {
                lock_guard lg(d->lock);

                if (d->name_len >= left)
                    break;

                memcpy(p - d->name_len, d->name, d->name_len);
                *(p - d->name_len - 1) = '/';

                p -= d->name_len + 1;
                left -= d->name_len + 1;
            }

            d = d->parent;

            if (d->name_len == 0) {
                if (left == 0)
                    done = true;
                break;
            }
        }

        if (done)
            break;

        free(s);
    }

    path = s;
    path_len = len;

    return nt_status::SUCCESS;
}

void file::last_handle_closed() {
    {
        lock_guard lg(dentry->lock);

        if (--dentry->open_file_count != 0)
            return;

        if (dentry->status != dentry_status::invalid)
            return;
    }

    dentry->inode->last_handle_closed();
}
