module;

#include <stdint.h>
#include <array>
#include <string_view>
#include <span>
#include <bit>
#include <optional>
#include <string.h>

export module ipv6;

import ws2_32;
import format;

using namespace std;

export struct ipv6_address {
    auto operator<=>(const ipv6_address& other) const {
        return lexicographical_compare_three_way(v.begin(), v.end(),
                                                 other.v.begin(), other.v.end());
    }

    constexpr bool operator==(const ipv6_address& other) const {
        return equal(v.begin(), v.end(), other.v.begin());
    }

    array<uint8_t, 16> v;
};

export struct ipv6_header {
    uint32_t traffic_class1 : 4;
    uint32_t version : 4;
    uint32_t traffic_class2 : 4;
    uint32_t flow_label : 20;
    uint16_t payload_length;
    uint8_t next_header;
    uint8_t hop_limit;
    ipv6_address src;
    ipv6_address dst;
} __attribute((packed));

static_assert(sizeof(ipv6_header) == 40);

export enum class icmpv6_type : uint8_t {
    dest_unreachable = 1,
    packet_too_big = 2,
    time_exceeded = 3,
    param_problem = 4,
    echo_request = 128,
    echo_reply = 129,
    router_solicitation = 133,
    router_advertisement = 134,
    neighbour_solicitation = 135,
    neighbour_advertisement = 136
};

export struct icmpv6_header {
    icmpv6_type type;
    uint8_t code;
    uint16_t checksum;
} __attribute((packed));

static_assert(sizeof(icmpv6_header) == 4);

export struct icmpv6_echo : icmpv6_header {
    uint16_t identifier;
    uint16_t sequence;
} __attribute((packed));

static_assert(sizeof(icmpv6_echo) == 8);

export struct icmpv6_error : icmpv6_header {
    uint32_t reserved;
    ipv6_header header;
} __attribute((packed));

static_assert(sizeof(icmpv6_error) == 48);

export enum icmpv6_dest_unreachable_code : uint8_t {
    no_route = 0,
    administratively_prohibited = 1,
    beyond_scope = 2,
    address_unreachable = 3,
    no_port = 4,
    policy_fail = 5,
    reject_route = 6,
};

export enum icmpv6_time_exceeded_code : uint8_t {
    hop_limit = 0,
    defragmentation_failure = 1,
};

export enum icmpv6_param_problem_code : uint8_t {
    wrong_header_field = 0,
    unknown_header = 1,
    unknown_option = 2,
};

template<>
consteval bool format_spec_valid<ipv6_address>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const ipv6_address& a) {
    unsigned int run_start = 0, run_length = 0;
    unsigned int largest_run_start = 0, largest_run_length = 0;

    static const uint8_t MAPPED_IPV4[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff };

    if (!memcmp(a.v.data(), MAPPED_IPV4, sizeof(MAPPED_IPV4))) {
        sp = do_format_arg(sp, "", "::ffff:");
        sp = do_format_arg(sp, "", a.v[12]);
        sp = do_format_arg(sp, "", ".");
        sp = do_format_arg(sp, "", a.v[13]);
        sp = do_format_arg(sp, "", ".");
        sp = do_format_arg(sp, "", a.v[14]);
        sp = do_format_arg(sp, "", ".");
        sp = do_format_arg(sp, "", a.v[15]);
        return sp;
    }

    for (unsigned int i = 0; i < 8; i++) {
        if (a.v[2 * i] == 0 && a.v[(2 * i) + 1] == 0) {
            if (run_start + run_length == i)
                run_length++;
            else {
                run_start = i;
                run_length = 1;
            }
        } else if (i == run_start + run_length) {
            if (run_length > largest_run_length) {
                largest_run_start = run_start;
                largest_run_length = run_length;
            }
        }
    }

    if (run_start + run_length == 8 && run_length > largest_run_length) {
        largest_run_start = run_start;
        largest_run_length = run_length;
    }

    if (largest_run_length == 1) {
        largest_run_start = 0;
        largest_run_length = 0;
    }

    for (unsigned int i = 0; i < 8; i++) {
        uint16_t num = (uint16_t)(a.v[2 * i] << 8) | a.v[(2 * i) + 1];

        if (i == largest_run_start && largest_run_length != 0) {
            sp = do_format_arg(sp, "", "::");
            i += largest_run_length - 1;
            continue;
        }

        if (i != 0 && i != largest_run_start + largest_run_length)
            sp = do_format_arg(sp, "", ":");

        sp = do_format_arg(sp, ":x", num);
    }

    return sp;
}

export uint16_t calc_icmpv6_checksum(const ipv6_header& ip, span<const uint8_t> icmp) {
    struct {
        ipv6_address src;
        ipv6_address dst;
        uint32_t length;
        uint8_t zero[3];
        uint8_t next_header;
    } __attribute__((packed)) pseudo;

    uint32_t sum = 0;

    pseudo.src = ip.src;
    pseudo.dst = ip.dst;
    pseudo.length = byteswap((uint32_t)icmp.size());
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    pseudo.next_header = IPPROTO_ICMPV6;

    for (auto s : span((uint16_t*)&pseudo, sizeof(pseudo) / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    for (auto s : span((uint16_t*)icmp.data(), icmp.size() / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    if (icmp.size() & 1)
        sum += (uint16_t)(icmp[icmp.size() - 1] << 8);

    while (sum & 0xffff0000) {
        sum = (sum >> 16) + (sum & 0xffff);
    }

    return byteswap((uint16_t)(~(uint16_t)sum));
}

export constexpr optional<ipv6_address> try_parse_ipv6_address(string_view s) {
    uint16_t parts[8];
    unsigned int num_parts = 0;
    optional<unsigned int> dc_pos;
    size_t i = 0;

    while (i < s.size() && num_parts < 8) {
        if (i < s.size() - 1 && s[i] == ':' && s[i+1] == ':') {
            if (dc_pos.has_value())
                return nullopt; // multiple ::

            dc_pos = num_parts;
            i += 2;

            if (i >= s.size())
                break;

            continue;
        }

        if (s[i] == ':')
            i++;

        uint16_t v = 0;

        for (unsigned int j = 0; j < 4; j++) {
            if (i == s.size()) {
                if (j == 0)
                    return nullopt;
                else
                    break;
            }

            uint16_t n;

            if (s[i] >= '0' && s[i] <= '9')
                n = (uint16_t)(s[i] - '0');
            else if (s[i] >= 'a' && s[i] <= 'f')
                n = (uint16_t)(s[i] - 'a' + 10);
            else if (s[i] >= 'A' && s[i] <= 'F')
                n = (uint16_t)(s[i] - 'A' + 10);
            else {
                if (j == 0)
                    return nullopt;
                else
                    break;
            }

            v <<= 4;
            v |= n;
            i++;
        }

        parts[num_parts] = v;
        num_parts++;
    }

    if (i < s.size())
        return nullopt;

    if (num_parts != 8 && !dc_pos.has_value())
        return nullopt;

    if (dc_pos.has_value()) {
        for (unsigned int i = num_parts; i > *dc_pos; i--) {
            parts[i - 1 + 8 - num_parts] = parts[i - 1];
        }

        for (unsigned int i = 0; i < 8 - num_parts; i++) {
            parts[*dc_pos + i] = 0;
        }

    }

    ipv6_address ret;

    for (unsigned int i = 0; i < 8; i++) {
        ret.v[2 * i] = (uint8_t)(parts[i] >> 8);
        ret.v[(2 * i) + 1] = (uint8_t)(parts[i] & 0xff);
    }

    return ret;
}

export consteval ipv6_address parse_ipv6_address(string_view s) {
    return *try_parse_ipv6_address(s);
}

static_assert(try_parse_ipv6_address("::").has_value());
static_assert(*try_parse_ipv6_address("::") == ipv6_address{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
static_assert(try_parse_ipv6_address("::1").has_value());
static_assert(*try_parse_ipv6_address("::1") == ipv6_address{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
static_assert(try_parse_ipv6_address("fe80::1").has_value());
static_assert(*try_parse_ipv6_address("fe80::1") == ipv6_address{0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
static_assert(try_parse_ipv6_address("2001:db8::1").has_value());
static_assert(*try_parse_ipv6_address("2001:db8::1") == ipv6_address{0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
static_assert(try_parse_ipv6_address("2a00:1450:4009:c08:0:0:0:66").has_value());
static_assert(*try_parse_ipv6_address("2a00:1450:4009:c08:0:0:0:66") == ipv6_address{0x2a, 0x00, 0x14, 0x50, 0x40, 0x09, 0x0c, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66});
static_assert(!try_parse_ipv6_address("").has_value());
static_assert(!try_parse_ipv6_address(":::").has_value());
static_assert(!try_parse_ipv6_address("1::2::3").has_value());
static_assert(!try_parse_ipv6_address("gggg::1").has_value());
