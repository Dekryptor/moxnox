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
