module;

#include <stdint.h>
#include <array>
#include <span>
#include <optional>
#include <string_view>
#include <utility>
#include <charconv>

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

export constexpr optional<ipv4_address> try_parse_ipv4_address(string_view sv) {
    uint32_t values[4];
    size_t num_parts = 0;

    // FIXME - in theory should be parsing hex or octal too

    for (size_t i = 0; i < 4; i++) {
        auto dot = sv.find('.');

        if (dot == 0)
            return nullopt;

        auto len = (dot == string_view::npos) ? sv.size() : dot;

        auto [ptr, ec] = from_chars(sv.data(), sv.data() + len, values[i]);

        if (ec != errc() || ptr != sv.data() + len)
            return nullopt;

        num_parts++;

        if (dot == string_view::npos) {
            sv = {};
            break;
        }

        sv = string_view(sv.data() + dot + 1, sv.size() - dot - 1);
        if (sv.empty())
            break;
    }

    if (!sv.empty())
        return nullopt;

    ipv4_address ret;

    if (num_parts == 1) {
        ret.v[0] = (uint8_t)(values[0] >> 24);
        ret.v[1] = (values[0] >> 16) & 0xff;
        ret.v[2] = (values[0] >> 8) & 0xff;
        ret.v[3] = values[0] & 0xff;
    } else if (num_parts == 2) {
        if (values[0] > 0xff || values[1] > 0xffffff)
            return nullopt;

        ret.v[0] = (uint8_t)values[0];
        ret.v[1] = (values[1] >> 16) & 0xff;
        ret.v[2] = (values[1] >> 8) & 0xff;
        ret.v[3] = values[1] & 0xff;
    } else if (num_parts == 3) {
        if (values[0] > 0xff || values[1] > 0xff || values[2] > 0xffff)
            return nullopt;

        ret.v[0] = (uint8_t)values[0];
        ret.v[1] = (uint8_t)values[1];
        ret.v[2] = (values[2] >> 8) & 0xff;
        ret.v[3] = values[2] & 0xff;
    } else if (num_parts == 4) {
        for (auto v : values) {
            if (v > 0xff)
                return nullopt;
        }

        ret.v[0] = (uint8_t)values[0];
        ret.v[1] = (uint8_t)values[1];
        ret.v[2] = (uint8_t)values[2];
        ret.v[3] = (uint8_t)values[3];
    } else
        unreachable();

    return ret;
}

export consteval ipv4_address parse_ipv4_address(string_view s) {
    return *try_parse_ipv4_address(s);
}

static_assert(try_parse_ipv4_address("0.0.0.0").has_value());
static_assert(*try_parse_ipv4_address("0.0.0.0") == ipv4_address{0, 0, 0, 0});
static_assert(try_parse_ipv4_address("127.0.0.1").has_value());
static_assert(*try_parse_ipv4_address("127.0.0.1") == ipv4_address{127, 0, 0, 1});
static_assert(try_parse_ipv4_address("203569230").has_value());
static_assert(*try_parse_ipv4_address("203569230") == ipv4_address{12, 34, 56, 78});
static_assert(try_parse_ipv4_address("12.2242638").has_value());
static_assert(*try_parse_ipv4_address("12.2242638") == ipv4_address{12, 34, 56, 78});
static_assert(try_parse_ipv4_address("12.34.14414").has_value());
static_assert(*try_parse_ipv4_address("12.34.14414") == ipv4_address{12, 34, 56, 78});
static_assert(!try_parse_ipv4_address("").has_value());
static_assert(!try_parse_ipv4_address(".").has_value());
static_assert(!try_parse_ipv4_address("256.0.0.1").has_value());
static_assert(!try_parse_ipv4_address("255.0.0.-1").has_value());
static_assert(!try_parse_ipv4_address("a.b.c.d").has_value());
