module;

#include <stdint.h>
#include <array>
#include <string_view>
#include <span>
#include <string.h>

export module ipv6;

import format;

using namespace std;

export struct ipv6_address {
    auto operator<=>(const ipv6_address& other) const {
        return lexicographical_compare_three_way(v.begin(), v.end(),
                                                 other.v.begin(), other.v.end());
    }

    bool operator==(const ipv6_address& other) const {
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

export struct icmpv6_neighbour_solicitation : icmpv6_header {
    uint32_t reserved;
    ipv6_address target;
} __attribute((packed));

static_assert(sizeof(icmpv6_neighbour_solicitation) == 24);

export struct icmpv6_neighbour_advertisement : icmpv6_header {
    uint32_t : 5;
    uint32_t _override : 1;
    uint32_t solicited : 1;
    uint32_t router : 1;
    uint32_t : 24;
    ipv6_address target;
} __attribute((packed));

static_assert(sizeof(icmpv6_neighbour_advertisement) == 24);

export enum class ndp_option_type : uint8_t {
    source_lladdr = 1,
    target_lladdr = 2,
    prefix_info = 3,
    redirected_header = 4,
    mtu = 5,
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
