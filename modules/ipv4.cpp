module;

#include <stdint.h>
#include <array>
#include <span>
#include <string_view>

export module ipv4;

import format;

using namespace std;

export struct ipv4_address {
    auto operator<=>(const ipv4_address& other) const {
        return lexicographical_compare_three_way(v.begin(), v.end(),
                                                 other.v.begin(), other.v.end());
    }

    constexpr bool operator==(const ipv4_address& other) const {
        return equal(v.begin(), v.end(), other.v.begin());
    }

    array<uint8_t, 4> v;
};

export struct ipv4_header {
    uint32_t ihl : 4;
    uint32_t version : 4;
    uint32_t ecn : 2;
    uint32_t dscp : 6;
    uint32_t total_length : 16;
    uint32_t identification : 16;
    uint32_t fragment_offset1 : 5;
    uint32_t flags : 3;
    uint32_t fragment_offset2 : 8;
    uint32_t ttl : 8;
    uint32_t protocol : 8;
    uint32_t header_checksum : 16;
    ipv4_address src;
    ipv4_address dst;
} __attribute((packed));

static_assert(sizeof(ipv4_header) == 20);

template<>
consteval bool format_spec_valid<ipv4_address>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const ipv4_address& a) {
    sp = do_format_arg(sp, "", a.v[0]);
    sp = do_format_arg(sp, "", ".");
    sp = do_format_arg(sp, "", a.v[1]);
    sp = do_format_arg(sp, "", ".");
    sp = do_format_arg(sp, "", a.v[2]);
    sp = do_format_arg(sp, "", ".");
    sp = do_format_arg(sp, "", a.v[3]);

    return sp;
}
