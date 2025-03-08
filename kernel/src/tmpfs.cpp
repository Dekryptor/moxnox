module;

#include "kernel.h"
#include "objects.h"
#include "volume.h"

export module tmpfs;

import print;

using namespace std;

class tmpfs_filesystem : public filesystem {
public:
    tmpfs_filesystem() : filesystem(nullptr) { }

    ~tmpfs_filesystem() override {
    }

    nt_status open(string_view fn, const optional<reference_wrapper<inode>>& dir,
                   object_ref& obj_ref, uint32_t create_disposition,
                   uint32_t create_options) override;
    nt_status flush(bool make_readonly) override;

private:
    bool readonly = false;
};

export tmpfs_filesystem* create_tmpfs() {
    auto fs = new tmpfs_filesystem();
    if (!fs)
        return nullptr;

    fs->refcount++;

    return fs;
}

nt_status tmpfs_filesystem::open(string_view fn,
                                 const optional<reference_wrapper<inode>>& dir,
                                 object_ref& obj_ref,
                                 uint32_t create_disposition,
                                 uint32_t create_options) {
    print("FIXME - tmpfs_filesystem::open({})\n", fn);

    return nt_status::NOT_IMPLEMENTED;
}

nt_status tmpfs_filesystem::flush(bool make_readonly) {
    if (make_readonly)
        readonly = true;

    return nt_status::SUCCESS;
}
