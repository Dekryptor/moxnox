module;

#include <stdint.h>
#include <span>

module trace;

import nt;
import chardev;
import print;
import handle;
import object_ref;

using namespace std;

class tracectl : public chardev {
public:
    tracectl() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

bool create_dev_tracectl() {
    auto pptr = new tracectl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "tracectl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}
