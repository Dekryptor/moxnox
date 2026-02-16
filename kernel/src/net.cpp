module;

#include <array>
#include <string_view>
#include <span>
#include <stdint.h>

export module net;

import format;

using namespace std;

export struct mac_address {
    array<uint8_t, 6> v;
};

export struct ethernet_header {
    mac_address dst_mac;
    mac_address src_mac;
    uint16_t ethertype;
} __attribute((packed));

static_assert(sizeof(ethernet_header) == 14);

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
