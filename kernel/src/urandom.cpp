// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <algorithm>
#include <stdint.h>

export module urandom;

import nt;
import print;
import chardev;
import handle;
import object_ref;
import vec;
import kernel_time_page;
import config;

using namespace std;

class random_chardev : public chardev {
public:
    random_chardev() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;
    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
};

export bool create_dev_urandom() {
    auto pptr = new random_chardev;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "urandom");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

nt_status random_chardev::write(span<const span<const uint8_t>> v,
                                uint64_t& written_bytes) {
    written_bytes = get_vec_size(v);

    return nt_status::SUCCESS;
}

nt_status random_chardev::read(span<const span<uint8_t>> v,
                               uint64_t& read_bytes) {
    auto len = get_vec_size(v);

    read_bytes = len;

    if (read_bytes == 0)
        return nt_status::SUCCESS;

    // FIXME - do this properly

    auto rnd = (uint64_t)(kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE));

    span<uint8_t> top = v.front();

    while (true) {
        rnd = rnd * (uint64_t)1103515245 + 12345; // from Knuth

        auto n = (uint32_t)rnd;

        memcpy_vec(v, top, (uint8_t*)&n, min(len, sizeof(uint32_t)));

        if (len > sizeof(uint32_t))
            len -= sizeof(uint32_t);
        else
            break;
    }

    return nt_status::SUCCESS;
}
