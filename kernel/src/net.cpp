module;

#include <string_view>
#include <span>
#include <bit>
#include <algorithm>
#include <atomic>
#include <stdint.h>

export module net;

import format;
import crt;
import nt;
import list;
import spinlock;
import objects;
import chardev;
import ws2_32;
import ipv6;

using namespace std;

export struct mac_address {
    auto operator<=>(const mac_address& other) const {
        return lexicographical_compare_three_way(v.begin(), v.end(),
                                                 other.v.begin(), other.v.end());
    }

    bool operator==(const mac_address& other) const {
        return equal(v.begin(), v.end(), other.v.begin());
    }

    array<uint8_t, 6> v;
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

export enum class ip_proto : uint8_t {
    icmpv4 = 1,
    tcp = 6,
    udp = 17,
    icmpv6 = 58
};

export struct ndp_option_lladdr {
    ndp_option_type type;
    uint8_t length;
    mac_address mac;
} __attribute((packed));

static_assert(sizeof(ndp_option_lladdr) == 8);

export struct ip_addr {
    list_entry le;
    ipv6_address addr;
};

export class netif {
public:
    // FIXME - GCC 15 errors if no ctor or dtor here
    netif() { }
    ~netif() = default;

    virtual nt_status send_packet(span<const uint8_t> buf) = 0;

    list_entry le;
    mac_address mac;
    uint16_t mtu;

    spinlock addr_lock;
    list_entry addrs = { &addrs, &addrs };
    ipv6_address ll_addr;
    uint32_t num;
};

export class socket : public sync_object {
public:
    socket() : sync_object(object_type::socket, false) { }
    virtual ~socket() { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written);

    virtual nt_status recvfrom(char* buf, int len, int flags, sockaddr* from,
                               int* fromlen, size_t& recv_bytes) = 0;
    virtual nt_status sendto(const char* buf, int len, int flags, sockaddr* to,
                             int tolen, size_t& sent_bytes) = 0;

private:
    nt_status do_recvfrom(span<const uint8_t> inbuf);
    nt_status do_sendto(span<const uint8_t> inbuf);
};

static_assert(has_ioctls<class socket>);
static_assert(is_sync_object<class socket>);

class ipv6_socket : public socket {
public:
    ipv6_socket(int type, int protocol) : type(type), protocol(protocol) { }
    ~ipv6_socket() override;
    nt_status recvfrom(char* buf, int len, int flags, sockaddr* from,
                       int* fromlen, size_t& recv_bytes) override;
    nt_status sendto(const char* buf, int len, int flags, sockaddr* to,
                     int tolen, size_t& sent_bytes) override;

    list_entry le;
    int type, protocol;
    bool added_to_list = false;
    spinlock buffer_lock;
    list_entry buffer = { &buffer, &buffer };
    size_t buffer_size = 0;

private:
    nt_status sendto_raw(span<const char> buf, int flags,
                         const sockaddr_in6& to, size_t& sent_bytes);
};

class netctl : public chardev {
public:
    netctl() { }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_IMPLEMENTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_IMPLEMENTED;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
};

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
consteval bool format_spec_valid<ip_proto>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, ip_proto p) {
    switch (p) {
        using enum ip_proto;

        case icmpv4:
            return do_format_arg(sp, "", "icmpv4");
        case tcp:
            return do_format_arg(sp, "", "tcp");
        case udp:
            return do_format_arg(sp, "", "udp");
        case icmpv6:
            return do_format_arg(sp, "", "icmpv6");
        default:
            return do_format_arg(sp, ":02x", (uint8_t)p);
    }

    return sp;
}

template<>
consteval bool format_spec_valid<icmpv6_type>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, icmpv6_type t) {
    switch (t) {
        using enum icmpv6_type;

        case dest_unreachable:
            return do_format_arg(sp, "", "dest_unreachable");
        case packet_too_big:
            return do_format_arg(sp, "", "packet_too_big");
        case time_exceeded:
            return do_format_arg(sp, "", "time_exceeded");
        case param_problem:
            return do_format_arg(sp, "", "param_problem");
        case echo_request:
            return do_format_arg(sp, "", "echo_request");
        case echo_reply:
            return do_format_arg(sp, "", "echo_reply");
        case router_solicitation:
            return do_format_arg(sp, "", "router_solicitation");
        case router_advertisement:
            return do_format_arg(sp, "", "router_advertisement");
        case neighbour_solicitation:
            return do_format_arg(sp, "", "neighbour_solicitation");
        case neighbour_advertisement:
            return do_format_arg(sp, "", "neighbour_advertisement");
        default:
            return do_format_arg(sp, ":02x", (uint8_t)t);
    }

    return sp;
}

export spinlock netif_lock;
export list_entry netif_list = { &netif_list, &netif_list };
export atomic<uint32_t> next_netif_num = 1;

export void handle_ipv6_packet(netif& ni, span<const uint8_t> buf);
export bool drop_packet(const mac_address& dst, const mac_address& us);
export bool init_netctl();
