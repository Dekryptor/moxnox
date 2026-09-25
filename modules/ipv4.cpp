module;

#include <stdint.h>
#include <array>
#include <span>
#include <optional>
#include <string_view>
#include <utility>
#include <charconv>
#include <bit>

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
    uint32_t MF : 1;
    uint32_t DF : 1;
    uint32_t R : 1;
    uint32_t fragment_offset2 : 8;
    uint32_t ttl : 8;
    uint32_t protocol : 8;
    uint32_t header_checksum : 16;
    ipv4_address src;
    ipv4_address dst;
} __attribute((packed));

static_assert(sizeof(ipv4_header) == 20);

export struct arp_header {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t opcode;
    uint8_t sender_mac_address[6];
    ipv4_address sender_ip_address;
    uint8_t target_mac_address[6];
    ipv4_address target_ip_address;
} __attribute((packed));

static_assert(sizeof(arp_header) == 28);

export enum class icmpv4_type : uint8_t {
    echo_reply = 0,
    dest_unreachable = 3,
    source_quench = 4,
    redirect_message = 5,
    echo_request = 8,
    router_advertisement = 9,
    router_solicitation = 10,
    time_exceeded = 11,
    param_problem = 12,
    timestamp = 13,
    timestamp_reply = 14,
    extended_echo_request = 42,
    extended_echo_reply = 43,
};

export struct icmpv4_header {
    icmpv4_type type;
    uint8_t code;
    uint16_t checksum;
} __attribute((packed));

static_assert(sizeof(icmpv4_header) == 4);

export struct icmpv4_echo : icmpv4_header {
    uint16_t identifier;
    uint16_t sequence;
} __attribute((packed));

static_assert(sizeof(icmpv4_echo) == 8);

export struct icmpv4_error : icmpv4_header {
    uint32_t param;
    ipv4_header header;
} __attribute((packed));

static_assert(sizeof(icmpv4_error) == 28);

export enum class icmpv4_dest_unreachable_code : uint8_t {
    dest_net_unreachable = 0,
    dest_host_unreachable = 1,
    dest_proto_unreachable = 2,
    dest_port_unreachable = 3,
    fragmentation_required = 4,
    src_route_failed = 5,
    dest_net_unknown = 6,
    dest_host_unknown = 7,
    src_host_isolated = 8,
    net_administratively_prohibited = 9,
    host_administratively_prohibited = 10,
    net_unreachable_for_tos = 11,
    host_unreachable_for_tos = 12,
    communication_administratively_prohibited = 13,
    host_precedence_violation = 14,
    precedence_cutoff_in_effect = 15,
};

export enum class icmpv4_time_exceeded_code : uint8_t {
    ttl_expired = 0,
    fragmentation_time_exceeded = 1,
};

export enum class dhcp_opcode : uint8_t {
    BOOTREQUEST = 1,
    BOOTREPLY = 2
};

export struct dhcp_message {
    enum dhcp_opcode op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    ipv4_address ciaddr;
    ipv4_address yiaddr;
    ipv4_address siaddr;
    ipv4_address giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t cookie;
} __attribute((packed));

static_assert(sizeof(dhcp_message) == 240);

export const uint32_t DHCP_COOKIE = 0x63538263;

export enum class dhcp_option : uint8_t {
    subnet_mask = 1,
    router = 3,
    dns = 6,
    lease_time = 51,
    dhcp_msg = 53,
    dhcp_server_id = 54,
    parameter_list = 55,
    dhcp_max_msg_size = 57,
    client_id = 61,
    end = 255,
};

export struct dhcp_tlv {
    enum dhcp_option type;
    uint8_t length;
} __attribute((packed));

export enum class dhcp_message_type : uint8_t {
     DHCPDISCOVER = 1,
     DHCPOFFER = 2,
     DHCPREQUEST = 3,
     DHCPDECLINE = 4,
     DHCPACK = 5,
     DHCPNAK = 6,
     DHCPRELEASE = 7,
     DHCPINFORM = 8,
};

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

export uint16_t calc_ipv4_checksum(span<const uint8_t> data) {
    uint32_t sum = 0;

    for (auto s : span((uint16_t*)data.data(), data.size() / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

    if (data.size() & 1)
        sum += (uint16_t)(data[data.size() - 1] << 8);

    while (sum & 0xffff0000) {
        sum = (sum >> 16) + (sum & 0xffff);
    }

    return byteswap((uint16_t)(~(uint16_t)sum));
}

template<>
consteval bool format_spec_valid<dhcp_option>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                dhcp_option arg) {
    switch (arg) {
        using enum dhcp_option;

        case subnet_mask:
            return do_format_arg(sp, "", "subnet_mask");
        case router:
            return do_format_arg(sp, "", "router");
        case dns:
            return do_format_arg(sp, "", "dns");
        case lease_time:
            return do_format_arg(sp, "", "lease_time");
        case dhcp_msg:
            return do_format_arg(sp, "", "dhcp_msg");
        case dhcp_server_id:
            return do_format_arg(sp, "", "dhcp_server_id");
        case parameter_list:
            return do_format_arg(sp, "", "parameter_list");
        case dhcp_max_msg_size:
            return do_format_arg(sp, "", "dhcp_max_msg_size");
        case client_id:
            return do_format_arg(sp, "", "client_id");
        case end:
            return do_format_arg(sp, "", "end");
        default:
            return do_format_arg(sp, "", (uint8_t)arg);
    }
}

template<>
consteval bool format_spec_valid<dhcp_message_type>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                dhcp_message_type arg) {
    switch (arg) {
        using enum dhcp_message_type;

        case DHCPDISCOVER:
            return do_format_arg(sp, "", "DHCPDISCOVER");
        case DHCPOFFER:
            return do_format_arg(sp, "", "DHCPOFFER");
        case DHCPREQUEST:
            return do_format_arg(sp, "", "DHCPREQUEST");
        case DHCPDECLINE:
            return do_format_arg(sp, "", "DHCPDECLINE");
        case DHCPACK:
            return do_format_arg(sp, "", "DHCPACK");
        case DHCPNAK:
            return do_format_arg(sp, "", "DHCPNAK");
        case DHCPRELEASE:
            return do_format_arg(sp, "", "DHCPRELEASE");
        case DHCPINFORM:
            return do_format_arg(sp, "", "DHCPINFORM");
        default:
            return do_format_arg(sp, "", (uint8_t)arg);
    }
}
