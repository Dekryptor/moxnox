module;

#include <optional>
#include <string_view>
#include <atomic>
#include <stdint.h>

export module vfs;

import nt;
import list;
import volume;
import object_ref;
import file;
import inode;
import spinlock;

using namespace std;

export extern "C++" class filesystem {
public:
    filesystem(volume* vol) : vol(vol) { }
    virtual ~filesystem() { }
    virtual nt_status open(string_view fn, const optional<reference_wrapper<inode>>& dir,
                           object_ref& obj_ref, uint32_t create_disposition,
                           uint32_t create_options, unsigned int& create_result) = 0;
    virtual nt_status flush(bool make_readonly) = 0;

    list_entry le;
    volume* vol;
    atomic<unsigned int> refcount = 0;
    dentry* root_dentry = nullptr;
    spinlock writeback_lock;
    list_entry writeback_list = { &writeback_list, &writeback_list };
};

export struct filesystem_recognizer {
    list_entry le;
    bool (*func)(volume&);
};

export list_entry filesystem_types = { &filesystem_types, &filesystem_types };

export void add_fs_to_list(::filesystem& fs);
export bool mount_root();
export nt_status open_inode(string_view fn, const optional<reference_wrapper<inode>>& dir,
                            object_ref& obj_ref, uint32_t create_disposition,
                            uint32_t create_options, unsigned int& create_result);
export nt_status flush_filesystems(bool make_readonly);
