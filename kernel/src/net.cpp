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
import object_ref;
import rwsem;

using namespace std;

export const size_t MTU = 1500; // FIXME - allow jumbo packets
const uint8_t DEFAULT_HOP_LIMIT = 64;

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

const mac_address NULL_MAC_ADDRESS = { 0, 0, 0, 0, 0, 0 };

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

export struct ip_addr {
    list_entry le;
    ipv6_address addr;
    bool slaac = false;
};

export struct packet {
    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    atomic<unsigned int> refcount = 1;
    size_t size;
    uint8_t data[MTU];
};

struct ipv6_socket_packet {
    ipv6_socket_packet(packet& pack) : pack(pack) {
        pack.acquire();
    }

    ~ipv6_socket_packet() {
        pack.release();
    }

    list_entry le;
    uint32_t scope_id;
    packet& pack;
};

export class netif {
public:
    // FIXME - GCC 15 errors if no ctor or dtor here
    netif() { }
    virtual ~netif();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    virtual nt_status send_packet(const mac_address& dst_mac, ethertype type,
                                  packet& pack) = 0;

    list_entry le;
    atomic<unsigned int> refcount = 1;
    mac_address mac;
    uint16_t mtu;

    spinlock addr_lock;
    list_entry addrs = { &addrs, &addrs };
    char name[16];
    ipv6_address ll_addr;
    uint32_t num;
    spinlock neighbour_lock;
    list_entry neighbour_cache = { &neighbour_cache, &neighbour_cache };
    bool rs_needed = true, rs_visited = false;
    unsigned int rs_tries = 0, secs_since_last_rs = 0;
    spinlock prefix_lock;
    list_entry prefixes = { &prefixes, &prefixes };
    spinlock routers_lock;
    list_entry routers = { &routers, &routers };
};

export class socket : public sync_object {
public:
    socket() : sync_object(object_type::socket, false) { }
    virtual ~socket() { }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written);
    virtual void last_handle_closed() = 0;

    virtual nt_status recvfrom(char* buf, size_t len, int flags, sockaddr* from,
                               int* fromlen, size_t& recv_bytes) = 0;
    virtual nt_status sendto(const char* buf, size_t len, int flags,
                             sockaddr* to, int tolen, size_t& sent_bytes) = 0;
    virtual nt_status bind(const sockaddr* addr, size_t addrlen) = 0;
    virtual nt_status connect(const sockaddr* addr, size_t addrlen) = 0;

private:
    nt_status do_recvfrom(span<const uint8_t> inbuf);
    nt_status do_sendto(span<const uint8_t> inbuf);
    nt_status do_bind(span<const uint8_t> inbuf);
    nt_status do_connect(span<const uint8_t> inbuf);
};

static_assert(has_ioctls<class socket>);
static_assert(is_sync_object<class socket>);
static_assert(has_last_handle_closed<class socket>);

enum class tcp_state {
    CLOSED,
    SYN_SENT,
    ESTABLISHED,
    CLOSE_WAIT,
    FIN_WAIT_1,
    FIN_WAIT_2,
    TIME_WAIT,
    LAST_ACK,
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
export atomic<uint32_t> next_eth_num = 0;

export void handle_ipv6_packet(netif& ni, packet& pack,
                               const mac_address& src_mac);
export bool drop_packet(const mac_address& dst, const mac_address& us);
export bool init_net();

void received_ipv6_tcp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac);
void received_ipv6_udp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac);
bool is_our_ip(const ipv6_address& ip);
nt_status dispatch_packet(netif& ni, packet& pack, const mac_address& dst_mac);
uint16_t calc_ipv6_checksum(const ipv6_header& ip, span<const uint8_t> data,
                            uint8_t next_header);
nt_status ipv6_choose_interface(const ipv6_address& dst, uint32_t scope_id,
                                netif*& ni, ipv6_address& src);
bool is_multicast_address(const ipv6_address& addr);
void send_address_unreachable(netif& ni, const packet& pack,
                              const ipv6_address& addr);
mac_address find_mac_address(netif& ni, const ipv6_address& dst);
void handle_icmpv6_error_udp(packet& pack);
nt_status create_new_ipv6_tcp_socket(object_ref& ref);
nt_status create_new_ipv6_udp_socket(object_ref& ref);
