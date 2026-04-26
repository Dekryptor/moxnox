module;

#include <span>
#include <stdint.h>

export module pty;

import nt;
import print;
import chardev;
import handle;
import object_ref;

using namespace std;

class ptyctl : public chardev {
public:
    ptyctl() { }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

export bool create_dev_ptyctl() {
    auto pptr = new ptyctl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "ptyctl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}
