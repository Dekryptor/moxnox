// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <algorithm>
#include <stdint.h>
#include <string.h>

export module vec;

using namespace std;

export size_t get_vec_size(span<const span<uint8_t>> v) {
    size_t ret = 0;

    for (auto s : v) {
        ret += s.size();
    }

    return ret;
}

export size_t get_vec_size(span<const span<const uint8_t>> v) {
    size_t ret = 0;

    for (auto s : v) {
        ret += s.size();
    }

    return ret;
}

export void memcpy_vec(span<const span<uint8_t>>& v, span<uint8_t>& top,
                       const uint8_t* src, size_t len) {
    while (len > 0) {
        if (top.empty()) {
            if (v.size() <= 1)
                return;

            v = v.subspan(1);
            top = v.front();
            continue;
        }

        size_t to_copy = min(top.size(), len);

        memcpy(top.data(), src, to_copy);

        top = top.subspan(to_copy);

        src += to_copy;
        len -= to_copy;
    }
}

export void memcpy_vec(uint8_t* dest, span<const span<const uint8_t>>& v,
                       span<const uint8_t>& top, size_t len) {
    while (len > 0) {
        if (top.empty()) {
            if (v.size() <= 1)
                return;

            v = v.subspan(1);
            top = v.front();
            continue;
        }

        size_t to_copy = min(top.size(), len);

        memcpy(dest, top.data(), to_copy);

        top = top.subspan(to_copy);

        dest += to_copy;
        len -= to_copy;
    }
}

export void memcpy_vec(span<const span<uint8_t>> dest,
                       span<const span<const uint8_t>> src) {
    if (dest.empty() || src.empty())
        return;

    auto dest_top = dest.front();
    auto src_top = src.front();

    while (true) {
        if (dest_top.empty()) {
            dest = dest.subspan(1);

            if (dest.empty())
                break;

            dest_top = dest.front();
            continue;
        }

        if (src_top.empty()) {
            src = src.subspan(1);

            if (src.empty())
                break;

            src_top = src.front();
            continue;
        }

        auto to_copy = min(dest_top.size(), src_top.size());

        memcpy(dest_top.data(), src_top.data(), to_copy);

        dest_top = dest_top.subspan(to_copy);
        src_top = src_top.subspan(to_copy);
    }
}
