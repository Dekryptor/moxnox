module;

#include <array>
#include <string_view>
#include <span>
#include <bit>
#include <mutex>
#include <stdint.h>

export module net;

import format;
import crt;
import print;
import mem;
import nt;
import list;
import spinlock;
import objects;
import object_ref;
import handle;
import chardev;

using namespace std;

static const uint8_t DEFAULT_HOP_LIMIT = 64;

export struct mac_address {
    auto operator<=>(const mac_address&) const = default;

    array<uint8_t, 6> v;
};

export struct ipv6_address {
    auto operator<=>(const ipv6_address&) const = default;

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

export enum class ip_proto : uint8_t {
    icmpv4 = 1,
    tcp = 6,
    udp = 17,
    icmpv6 = 58
};

export struct ipv6_header {
    uint32_t traffic_class1 : 4;
    uint32_t version : 4;
    uint32_t traffic_class2 : 4;
    uint32_t flow_label : 20;
    uint16_t payload_length;
    ip_proto next_header;
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

export class network_interface {
public:
    // FIXME - GCC 15 errors if no ctor or dtor here
    network_interface() { }
    ~network_interface() = default;

    virtual nt_status send_packet(span<const uint8_t> buf) = 0;

    mac_address mac;
    uint16_t mtu;

    spinlock addr_lock;
    list_entry addrs = { &addrs, &addrs };
    ipv6_address ll_addr;
};

export class socket : public object {
public:
    socket() : object(object_type::socket) { }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }
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

    nt_status ioctl(uint32_t /*code*/, span<const uint8_t> /*inbuf*/,
                    span<uint8_t> /*outbuf*/, uintptr_t& /*written*/) override {
        return nt_status::NOT_IMPLEMENTED;
    }
};

static object_ref netctl_obj;

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

static uint16_t calc_icmpv6_checksum(const ipv6_header& ip, span<const uint8_t> icmp) {
    struct {
        ipv6_address src;
        ipv6_address dst;
        uint32_t length;
        uint8_t zero[3];
        ip_proto next_header;
    } __attribute__((packed)) pseudo;

    uint32_t sum = 0;

    pseudo.src = ip.src;
    pseudo.dst = ip.dst;
    pseudo.length = byteswap((uint32_t)icmp.size());
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    pseudo.next_header = ip_proto::icmpv6;

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

static void send_neighbour_advertisement(network_interface& netif,
                                         const ipv6_address& target,
                                         const ipv6_address& dst_ip,
                                         const mac_address& dst_mac) {
    struct {
        ethernet_header eth;
        ipv6_header ip;
        icmpv6_neighbour_advertisement na;
        ndp_option_lladdr opt;
    } __attribute__((packed)) pkt;

    memset(&pkt, 0, sizeof(pkt));

    pkt.eth.src_mac = netif.mac;
    pkt.eth.dst_mac = dst_mac;

    pkt.eth.type = ethertype::ipv6;

    pkt.ip.version = 6;
    pkt.ip.payload_length = byteswap((uint16_t)(sizeof(icmpv6_neighbour_advertisement) + sizeof(ndp_option_lladdr)));
    pkt.ip.next_header = ip_proto::icmpv6;
    pkt.ip.hop_limit = 255;
    pkt.ip.src = netif.ll_addr;
    pkt.ip.dst = dst_ip;

    pkt.na.type = icmpv6_type::neighbour_advertisement;
    pkt.na.solicited = 1;
    pkt.na._override = 1;
    pkt.na.target = target;

    pkt.opt.type = ndp_option_type::target_lladdr;
    pkt.opt.length = 1; // 8 bytes
    pkt.opt.mac = netif.mac;

    pkt.na.checksum = calc_icmpv6_checksum(pkt.ip, span((uint8_t*)&pkt.na, sizeof(pkt.na) + sizeof(pkt.opt)));

    auto status = netif.send_packet(span((const uint8_t*)&pkt, sizeof(pkt)));
    if (!success(status))
        print("send_packet returned {}\n", status);
}

static void handle_neighbour_solicitation(network_interface& netif,
                                          span<const uint8_t> buf) {
    if (buf.size() < sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation)) {
        print("handle_neighbour_solicitation: buffer length was {}, expected at least {}\n",
              buf.size(), sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation));
        return;
    }

    auto& eth = *(ethernet_header*)buf.data();
    auto& ip = *(ipv6_header*)(buf.data() + sizeof(ethernet_header));
    auto& ns = *(icmpv6_neighbour_solicitation*)(buf.data() + sizeof(ethernet_header) + sizeof(ipv6_header));

#if 0
    print("ns: target = {}\n", ns.target);
#endif

    // FIXME - options (sender's MAC means we can avoid sending NS back to them)

    bool found = false;
    {
        lock_guard lock(netif.addr_lock);

        for (auto le = netif.addrs.next; le != &netif.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            if (a.addr == ns.target) {
                found = true;
                break;
            }
        }
    }

    if (!found)
        return;

    // FIXME - if source was :: (DAD), send to 33:33:00:00:00:01 and ff02::1

    send_neighbour_advertisement(netif, ns.target, ip.src, eth.src_mac);
}

static void handle_v6_echo_request(network_interface& netif,
                                   span<const uint8_t> buf) {
    const auto& orig_ip = *(ipv6_header*)(buf.data() + sizeof(ethernet_header));

    // check if it's for us

    bool found = false;
    {
        lock_guard lock(netif.addr_lock);

        for (auto le = netif.addrs.next; le != &netif.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            if (a.addr == orig_ip.dst) {
                found = true;
                break;
            }
        }
    }

    if (!found)
        return;

    // take copy

    auto reply = (uint8_t*)malloc(buf.size());
    if (!reply) {
        print("malloc failed\n");
        return;
    }

    memcpy(reply, buf.data(), buf.size());

    // switch MAC addresses

    auto& eth = *(ethernet_header*)reply;
    swap(eth.src_mac, eth.dst_mac);

    // switch IP addresses

    auto& ip = *(ipv6_header*)(reply + sizeof(ethernet_header));
    swap(ip.src, ip.dst);

    ip.hop_limit = DEFAULT_HOP_LIMIT;

    // change type to reply

    auto& header = *(icmpv6_header*)(reply + sizeof(ethernet_header) + sizeof(ipv6_header));
    header.type = icmpv6_type::echo_reply;
    header.checksum = 0;

    header.checksum = calc_icmpv6_checksum(ip, span((uint8_t*)&header, buf.size() - sizeof(ethernet_header) - sizeof(ipv6_header)));

    auto status = netif.send_packet(span(reply, buf.size()));
    if (!success(status))
        print("send_packet returned {}\n", status);

    free(reply);
}

static void handle_icmpv6_packet(network_interface& netif, span<const uint8_t> buf) {
    if (buf.size() < sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_header)) {
        print("handle_icmpv6_packet: buffer length was {}, expected at least {}\n",
              buf.size(), sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_header));
        return;
    }

    auto& header = *(icmpv6_header*)(buf.data() + sizeof(ethernet_header) + sizeof(ipv6_header));

#if 0
    print("icmpv6: type = {}, code = {:02x}, checksum = {:04x}\n",
          header.type, header.code, byteswap(header.checksum));
#endif

    switch (header.type) {
        case icmpv6_type::neighbour_solicitation:
            handle_neighbour_solicitation(netif, buf);
            break;

        case icmpv6_type::echo_request:
            handle_v6_echo_request(netif, buf);
            break;

        default:
            break;
    }
}

export void handle_ipv6_packet(network_interface& netif, span<const uint8_t> buf) {
    if (buf.size() < sizeof(ethernet_header) + sizeof(ipv6_header)) {
        print("handle_ipv6_packet: buffer length was {}, expected at least {}\n",
              buf.size(), sizeof(ethernet_header) + sizeof(ipv6_header));
        return;
    }

    auto& header = *(ipv6_header*)(buf.data() + sizeof(ethernet_header));

#if 0
    print("ipv6: version = {}, traffic_class1 = {}, traffic_class2 = {}, flow_label = {}, payload_length = {}, next_header = {}, hop_limit = {}, src = {}, dst = {}\n",
          header.version, header.traffic_class1, header.traffic_class2, header.flow_label,
          byteswap(header.payload_length), header.next_header, header.hop_limit,
          header.src, header.dst);
#endif

    switch (header.next_header) {
        case ip_proto::icmpv6:
            handle_icmpv6_packet(netif, buf);
            break;

        default:
            break;
    }
}

export bool drop_packet(const mac_address& dst, const mac_address& us) {
    if (dst == us)
        return false;

    // IPv6 multicast
    if (dst.v[0] == 0x33 && dst.v[1] == 0x33)
        return false;

    // broadcast
    if (dst.v[0] == 0xff && dst.v[1] == 0xff && dst.v[2] == 0xff &&
        dst.v[3] == 0xff && dst.v[4] == 0xff && dst.v[5] == 0xff) {
        return false;
    }

    return true;
}

export bool init_netctl() {
    auto pptr = new netctl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    netctl_obj = object_ref{*pptr};

    auto status = name_object(netctl_obj.get(), "netctl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}
