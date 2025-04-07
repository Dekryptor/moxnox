module;

#include "kernel.h"
import objects;
import event;
import chardev;

// FIXME - from handle.h
bool name_object(object& obj, std::string_view name, bool persistent = true);

export module null;

import print;

using namespace std;

class null_chardev : public chardev {
public:
    null_chardev() { }
    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
};

export bool create_dev_null() {
    auto pptr = new null_chardev;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    if (!name_object(p.get(), "null")) {
        print("name_object failed\n");
        return false;
    }

    return true;
}

nt_status null_chardev::read(span<uint8_t>, uint64_t& read_bytes) {
    read_bytes = 0;

    return nt_status::SUCCESS;
}

nt_status null_chardev::write(span<const uint8_t> s, uint64_t& written_bytes) {
    written_bytes = s.size();

    return nt_status::SUCCESS;
}
