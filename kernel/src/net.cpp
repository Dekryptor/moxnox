module;

#include <array>
#include <string_view>
#include <span>
#include <bit>
#include <stdint.h>

export module net;

import format;
import crt;

using namespace std;

export struct mac_address {
    array<uint8_t, 6> v;
};

export struct ipv6_address {
    array<uint8_t, 16> v;
};

export enum class ethertype : uint16_t { // bytes flipped
    ipv4 = 0x0008,
    arp = 0x0608,
    ipv6 = 0xdd86,
};

export struct ethernet_header {
    mac_address dst_mac;
    mac_address src_mac;
    ethertype type;
} __attribute((packed));

static_assert(sizeof(ethernet_header) == 14);

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

template<>
consteval bool format_spec_valid<mac_address>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const mac_address& m) {
    sp = do_format_arg(sp, ":02x", m.v[0]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[1]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[2]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[3]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[4]);
    sp = do_format_arg(sp, "", ":");
    sp = do_format_arg(sp, ":02x", m.v[5]);

    return sp;
}

template<>
consteval bool format_spec_valid<ethertype>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, ethertype e) {
    switch (e) {
        using enum ethertype;

        case ipv4:
            return do_format_arg(sp, "", "ipv4");
        case arp:
            return do_format_arg(sp, "", "arp");
        case ipv6:
            return do_format_arg(sp, "", "ipv6");
        default:
            return do_format_arg(sp, ":04x", byteswap((uint16_t)e));
    }

    return sp;
}

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
