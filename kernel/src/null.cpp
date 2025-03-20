module;

#include "kernel.h"
#include "objects.h"
#include "event.h"
#include "pipe.h"
#include "handle.h"

export module null;

import print;

using namespace std;

class null_pipe : public pipe {
public:
    null_pipe() : pipe(0) { }
    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;
    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
};

export bool create_dev_null() {
    auto pptr = new null_pipe;
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

nt_status null_pipe::read(span<uint8_t>, uint64_t& read_bytes) {
    read_bytes = 0;

    return nt_status::SUCCESS;
}

nt_status null_pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    written_bytes = s.size();

    return nt_status::SUCCESS;
}
