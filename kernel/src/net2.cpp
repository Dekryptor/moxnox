module;

#include <array>
#include <span>
#include <mutex>
#include <bit>
#include <atomic>
#include <optional>
#include <utility>
#include <string_view>
#include <stdint.h>

module net;

import bsod;
import print;
import handle;
import mem;
import moxnox_ioctls;
import thread;
import config;
import scheduler;
import event;
import kernel_time_page;
import timer;
import format;

using namespace std;

static const size_t MAX_BUFFER_SIZE = 1048576;

static object_ref netctl_obj;

static spinlock ipv6_raw_sockets_lock;
static list_entry ipv6_raw_sockets = { &ipv6_raw_sockets, &ipv6_raw_sockets };
static size_t num_ipv6_raw_sockets = 0;

class ipv6_raw_socket : public socket {
public:
    ipv6_raw_socket(int protocol) : protocol(protocol) { }
    ~ipv6_raw_socket() override;
    void last_handle_closed() override;

    nt_status recvfrom(char* buf, size_t len, int flags, sockaddr* from,
                       int* fromlen, size_t& recv_bytes) override;
    nt_status sendto(const char* buf, size_t len, int flags, sockaddr* to,
                     int tolen, size_t& sent_bytes) override;
    nt_status bind(const sockaddr* addr, size_t addrlen) override;

    nt_status connect(const sockaddr*, size_t) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status listen(int) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status accept(HANDLE&, void*, int&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    list_entry le;
    int protocol;
    bool added_to_list = false;
    spinlock buffer_lock;
    list_entry buffer = { &buffer, &buffer };
    size_t buffer_size = 0;
    spinlock bind_lock;
    bool bind_done = false;
    ipv6_address bound_address;
    nt_status pending_error = nt_status::SUCCESS;
    bool dead = false;
};

enum class neighbour_cache_status {
    incomplete,
    reachable
};

struct neighbour_cache_entry {
    neighbour_cache_entry(netif& ni) : ni(ni) {
        ni.acquire();
    }

    ~neighbour_cache_entry() {
        ni.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    list_entry le;
    netif& ni;
    ipv6_address addr;
    atomic<unsigned int> refcount = 1;
    object_ref event;
    mac_address mac;
    neighbour_cache_status status;
    int64_t last_update_time;
};

struct ipv6_route {
    ipv6_route(netif& ni, const ipv6_address& prefix, uint8_t prefix_length) : ni(ni), prefix(prefix), prefix_length(prefix_length) {
        ni.acquire();
    }

    ~ipv6_route() {
        ni.release();
    }

    list_entry le;
    netif& ni;
    ipv6_address prefix;
    uint8_t prefix_length;
};

struct ipv6_prefix {
    list_entry le;
    ipv6_address prefix;
    uint8_t prefix_length;
    uint32_t preferred_lifetime;
    uint32_t valid_lifetime;
    bool tick_visited = false;
    bool on_link;
};

struct ipv6_router {
    list_entry le;
    ipv6_address addr;
    mac_address mac;
    uint32_t lifetime;
    bool tick_visited = false;
};

struct icmpv6_neighbour_solicitation : icmpv6_header {
    uint32_t reserved;
    ipv6_address target;
} __attribute((packed));

static_assert(sizeof(icmpv6_neighbour_solicitation) == 24);

struct icmpv6_neighbour_advertisement : icmpv6_header {
    uint32_t : 5;
    uint32_t _override : 1;
    uint32_t solicited : 1;
    uint32_t router : 1;
    uint32_t : 24;
    ipv6_address target;
} __attribute((packed));

static_assert(sizeof(icmpv6_neighbour_advertisement) == 24);

struct icmpv6_router_solicitation : icmpv6_header {
    uint32_t reserved;
} __attribute((packed));

static_assert(sizeof(icmpv6_router_solicitation) == 8);

struct icmpv6_router_advertisement : icmpv6_header {
    uint8_t cur_hop_limit;
    uint8_t : 1;
    uint8_t snac_router : 1;
    uint8_t nd_proxy : 1;
    uint8_t prf : 2;
    uint8_t home_agent : 1;
    uint8_t other_configuration : 1;
    uint8_t managed_address_configuration : 1;
    uint16_t router_lifetime;
    uint32_t reachable_time;
    uint32_t retrans_timer;
} __attribute((packed));

static_assert(sizeof(icmpv6_router_advertisement) == 16);

enum class ndp_option_type : uint8_t {
    source_lladdr = 1,
    target_lladdr = 2,
    prefix_info = 3,
    redirected_header = 4,
    mtu = 5,
};

struct ndp_option {
    ndp_option_type type;
    uint8_t length;
} __attribute((packed));

static_assert(sizeof(ndp_option) == 2);

struct ndp_option_lladdr : ndp_option {
    mac_address mac;
} __attribute((packed));

static_assert(sizeof(ndp_option_lladdr) == 8);

struct ndp_option_prefix_info : ndp_option  {
    uint8_t prefix_length;
    uint8_t : 4;
    uint8_t dhcpv6_pd_preferred : 1;
    uint8_t router_address : 1;
    uint8_t autonomous : 1;
    uint8_t onlink : 1;
    uint32_t valid_lifetime;
    uint32_t preferred_lifetime;
    uint32_t reserved;
    ipv6_address prefix;
} __attribute__((packed));

static_assert(sizeof(ndp_option_prefix_info) == 32);

class loopback_netif : public netif {
public:
    nt_status send_packet(const mac_address& dst_mac, ethertype type,
                          packet& pack) override;
};

static spinlock ipv6_routes_lock;
static list_entry ipv6_routes = { &ipv6_routes, &ipv6_routes };

uint16_t calc_ipv6_checksum(const ipv6_header& ip, span<const uint8_t> data,
                            uint8_t next_header) {
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
    pseudo.length = byteswap((uint32_t)data.size());
    pseudo.zero[0] = 0;
    pseudo.zero[1] = 0;
    pseudo.zero[2] = 0;
    pseudo.next_header = next_header;

    for (auto s : span((uint16_t*)&pseudo, sizeof(pseudo) / sizeof(uint16_t))) {
        sum += byteswap(s);
    }

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

static void send_neighbour_advertisement(netif& ni, const ipv6_address& target,
                                         const ipv6_address& dst_ip,
                                         const mac_address& dst_mac) {
    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    struct na_pkt {
        ipv6_header ip;
        icmpv6_neighbour_advertisement na;
        ndp_option_lladdr opt;
    } __attribute__((packed));

    pack->size = sizeof(na_pkt);
    auto& pkt = *(na_pkt*)pack->data;

    memset(&pkt, 0, sizeof(pkt));

    pkt.ip.version = 6;
    pkt.ip.payload_length = byteswap((uint16_t)(sizeof(icmpv6_neighbour_advertisement) + sizeof(ndp_option_lladdr)));
    pkt.ip.next_header = (uint8_t)ip_proto::icmpv6;
    pkt.ip.hop_limit = 255;
    pkt.ip.src = ni.ll_addr;
    pkt.ip.dst = dst_ip;

    pkt.na.type = icmpv6_type::neighbour_advertisement;
    pkt.na.solicited = 1;
    pkt.na._override = 1;
    pkt.na.target = target;

    pkt.opt.type = ndp_option_type::target_lladdr;
    pkt.opt.length = 1; // 8 bytes
    pkt.opt.mac = ni.mac;

    pkt.na.checksum = calc_ipv6_checksum(pkt.ip, span((uint8_t*)&pkt.na, sizeof(pkt.na) + sizeof(pkt.opt)),
                                         IPPROTO_ICMPV6);

    auto status = ni.send_packet(dst_mac, ethertype::ipv6, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();
}

static bool valid_ndp_packet(const packet& pack) {
    const auto& ip = *(ipv6_header*)pack.data;

    if (ip.hop_limit != 255)
        return false;

    const auto& icmp = *(icmpv6_header*)(pack.data + sizeof(ipv6_header));

    if (calc_ipv6_checksum(ip, span((uint8_t*)&icmp, pack.size - sizeof(ipv6_header)), IPPROTO_ICMPV6) != 0)
        return false;

    return true;
}

static void handle_neighbour_solicitation(netif& ni, packet& pack,
                                          const mac_address& src_mac) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation)) {
        print("handle_neighbour_solicitation: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation));
        return;
    }

    if (!valid_ndp_packet(pack))
        return;

    auto& ip = *(ipv6_header*)pack.data;
    auto& ns = *(icmpv6_neighbour_solicitation*)(pack.data + sizeof(ipv6_header));

#if 0
    print("ns: target = {}\n", ns.target);
#endif

    // FIXME - options (sender's MAC means we can avoid sending NS back to them)

    bool found = false;
    {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
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

    send_neighbour_advertisement(ni, ns.target, ip.src, src_mac);
}

static void add_neighbour_cache_entry(netif& ni, const mac_address& mac,
                                      const ipv6_address& addr,
                                      bool do_override) {
    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr != addr)
                continue;

            if (!do_override && c.status == neighbour_cache_status::reachable)
                return;

            c.mac = mac;

            if (c.status == neighbour_cache_status::incomplete) {
                c.status = neighbour_cache_status::reachable;
                c.last_update_time = kernel_time_page.system_time;
                set_event(static_cast<event&>(c.event.get()));
            }

            return;
        }
    }

    // add new entry

    auto nce = new neighbour_cache_entry(ni);
    if (!nce) {
        print("new failed\n");
        return;
    }

    nce->addr = addr;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return;
    }

    set_event(static_cast<event&>(nce->event.get()));

    nce->status = neighbour_cache_status::reachable;

    bool beaten_to_it = false;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr == addr) {
                beaten_to_it = true;

                c.mac = mac;

                if (c.status == neighbour_cache_status::incomplete) {
                    c.status = neighbour_cache_status::reachable;
                    set_event(static_cast<event&>(c.event.get()));
                }

                c.last_update_time = kernel_time_page.system_time;

                break;
            }
        }

        if (!beaten_to_it) {
            insert_tail_list(ni.neighbour_cache, nce->le);
            nce->last_update_time = kernel_time_page.system_time;
        }
    }

    if (beaten_to_it)
        nce->release();
}

static void handle_neighbour_advertisement(netif& ni, packet& pack) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement)) {
        print("handle_neighbour_advertisement: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement));
        return;
    }

    if (!valid_ndp_packet(pack))
        return;

    auto& na = *(icmpv6_neighbour_advertisement*)(pack.data + sizeof(ipv6_header));

#if 0
    print("na: override = {}, solicited = {}, router = {}, target = {}\n",
          na._override, na.solicited, na.router, na.target);
#endif

    auto opts = span(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement),
                     pack.size - sizeof(ipv6_header) - sizeof(icmpv6_neighbour_advertisement));

    mac_address mac = NULL_MAC_ADDRESS;

    while (opts.size() >= 8) {
        auto& opt = *(ndp_option*)opts.data();

        if (opts.size() < opt.length * 8)
            break;

        if (opt.type == ndp_option_type::target_lladdr) {
            auto& opt2 = *(ndp_option_lladdr*)&opt;

            mac = opt2.mac;

            break;
        }

        opts = span(opts.data() + (opt.length * 8), opts.size() - (opt.length * 8));
    }

    if (mac == NULL_MAC_ADDRESS) {
        print("neighbour advertisement did not provide a MAC address\n");
        return;
    }

    // FIXME - if !na.solicited, update cache status to stale

    add_neighbour_cache_entry(ni, mac, na.target, na._override);
}

bool is_multicast_address(const ipv6_address& addr) {
    static const auto multicast_range = parse_ipv6_address("ff00::");

    return !memcmp(multicast_range.v.data(), addr.v.data(), 1);
}

static void handle_v6_echo_request(netif& ni, packet& pack,
                                   const mac_address& src_mac) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_echo)) {
        print("handle_v6_echo_request: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_echo));
        return;
    }

    const auto& orig_ip = *(ipv6_header*)pack.data;
    const auto& icmp = *(icmpv6_header*)(pack.data + sizeof(ipv6_header));

    if (calc_ipv6_checksum(orig_ip, span((uint8_t*)&icmp, pack.size - sizeof(ipv6_header)), IPPROTO_ICMPV6) != 0)
        return;

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    // take copy

    pack2->size = pack.size;
    memcpy(pack2->data, pack.data, pack.size);

    // switch IP addresses

    auto& ip = *(ipv6_header*)pack2->data;
    swap(ip.src, ip.dst);

    if (is_multicast_address(ip.src))
        ip.src = ni.ll_addr;

    ip.hop_limit = DEFAULT_HOP_LIMIT;

    // change type to reply

    auto& header = *(icmpv6_header*)(pack2->data + sizeof(ipv6_header));
    header.type = icmpv6_type::echo_reply;
    header.checksum = 0;

    header.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&header, pack2->size - sizeof(ipv6_header)), IPPROTO_ICMPV6);

    if (ip.src == ip.dst) // pinging ourselves
        handle_ipv6_packet(ni, *pack2, ni.mac);
    else {
        auto status = ni.send_packet(src_mac, ethertype::ipv6, *pack2);
        if (!success(status))
            print("send_packet returned {}\n", status);
    }

    pack2->release();
}

static void add_ipv6_route(netif& ni, const ipv6_address& prefix,
                           uint8_t prefix_length) {
    {
        lock_guard lock(ipv6_routes_lock);

        for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
            auto& r = containing_record<ipv6_route>(le);

            if (r.prefix == prefix && r.prefix_length == prefix_length &&
                &r.ni == &ni) {
                return;
            }
        }
    }

    auto r = new ipv6_route(ni, prefix, prefix_length);
    if (!r) {
        print("new failed\n");
        return;
    }

    bool beaten_to_it = false;

    {
        lock_guard lock(ipv6_routes_lock);

        for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
            auto& r2 = containing_record<ipv6_route>(le);

            if (r2.prefix == prefix && r2.prefix_length == prefix_length &&
                &r2.ni == &ni) {
                beaten_to_it = true;
                break;
            }
        }

        if (!beaten_to_it)
            insert_tail_list(ipv6_routes, r->le);
    }

    if (beaten_to_it)
        delete r;
}

static void remove_ipv6_route(netif& ni, const ipv6_address& prefix,
                              uint8_t prefix_length) {
    ipv6_route* found = nullptr;

    {
        lock_guard lock(ipv6_routes_lock);

        for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
            auto& r = containing_record<ipv6_route>(le);

            if (r.prefix == prefix && r.prefix_length == prefix_length &&
                &r.ni == &ni) {
                found = &r;
                remove_entry_list(r.le);
                break;
            }
        }
    }

    if (found)
        delete found;
}

template<>
consteval bool format_spec_valid<netif>(string_view fmt) {
    return fmt.empty();
}

static span<char> do_format_arg(span<char> sp, string_view, const netif& ni) {
    auto sv = string_view(ni.name, sizeof(ni.name));

    if (auto zero = sv.find('\0'); zero != string_view::npos)
        sv = string_view(sv.data(), zero);

    return do_format_arg(sp, "", sv);
}

static void add_ipv6_prefix(netif& ni, const ipv6_address& prefix,
                            uint8_t prefix_length, uint32_t valid_lifetime,
                            uint32_t preferred_lifetime, bool on_link,
                            bool autonomous) {
    ipv6_prefix* found = nullptr;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            if (p.prefix == prefix && p.prefix_length == prefix_length) {
                if (valid_lifetime == 0) {
                    found = &p;
                    remove_entry_list(p.le);
                    break;
                }

                p.valid_lifetime = valid_lifetime;
                p.preferred_lifetime = preferred_lifetime;
                p.on_link = on_link;
                return;
            }
        }
    }

    if (found) {
        delete found;
        remove_ipv6_route(ni, prefix, prefix_length);

        if (prefix_length == 64) {
            // delete SLAAC addresses

            while (true) {
                ip_addr* dead_addr = nullptr;

                {
                    lock_guard lock(ni.addr_lock);

                    for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                        auto& a = containing_record<ip_addr>(le);

                        if (!a.slaac)
                            continue;

                        if (!memcmp(a.addr.v.data(), prefix.v.data(), 8)) {
                            remove_entry_list(a.le);
                            dead_addr = &a;
                            break;
                        }
                    }
                }

                if (!dead_addr)
                    break;

                print("{}: removing IPv6 address {}\n", ni, dead_addr->addr);
                delete dead_addr;
            }
        }

        return;
    }

    if (valid_lifetime == 0)
        return;

    auto p = new ipv6_prefix;
    if (!p) {
        print("new failed\n");
        return;
    }

    p->prefix = prefix;
    p->prefix_length = prefix_length;
    p->preferred_lifetime = preferred_lifetime;
    p->valid_lifetime = valid_lifetime;
    p->on_link = on_link;

    bool beaten_to_it = false;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p2 = containing_record<ipv6_prefix>(le);

            if (p2.prefix == prefix && p2.prefix_length == prefix_length) {
                p2.valid_lifetime = valid_lifetime;
                p2.preferred_lifetime = preferred_lifetime;
                p2.on_link = on_link;
                beaten_to_it = true;
                break;
            }
        }

        if (!beaten_to_it)
            insert_tail_list(ni.prefixes, p->le);
    }

    if (prefix_length == 64) {
        if (autonomous) {
            ipv6_address slaac;

            memcpy(slaac.v.data(), prefix.v.data(), 8);
            memcpy(&slaac.v[8], &ni.ll_addr.v[8], 8);

            bool already_exists = false;

            {
                lock_guard lock(ni.addr_lock);

                for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                    auto& a = containing_record<ip_addr>(le);

                    if (a.addr == slaac) {
                        already_exists = true;
                        break;
                    }
                }
            }

            if (!already_exists) {
                auto a = new ip_addr;
                if (!a)
                    print("new failed\n");
                else {
                    a->addr = slaac;
                    a->slaac = true;

                    {
                        lock_guard lock(ni.addr_lock);

                        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                            auto& a2 = containing_record<ip_addr>(le);

                            if (a2.addr == slaac) {
                                already_exists = true;
                                break;
                            }
                        }

                        if (!already_exists)
                            insert_tail_list(ni.addrs, a->le);
                    }

                    if (already_exists)
                        delete a;
                    else
                        print("{}: added new IPv6 address {}\n", ni, slaac);
                }
            }
        } else {
            // delete SLAAC addresses

            while (true) {
                ip_addr* dead_addr = nullptr;

                {
                    lock_guard lock(ni.addr_lock);

                    for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                        auto& a = containing_record<ip_addr>(le);

                        if (!a.slaac)
                            continue;

                        if (!memcmp(a.addr.v.data(), prefix.v.data(), 8)) {
                            remove_entry_list(a.le);
                            dead_addr = &a;
                            break;
                        }
                    }
                }

                if (!dead_addr)
                    break;

                print("{}: removing IPv6 address {}\n", ni, dead_addr->addr);
                delete dead_addr;
            }
        }
    }

    if (beaten_to_it) {
        delete p;
        return;
    }

    add_ipv6_route(ni, prefix, prefix_length);
}

static ipv6_address zero_right_bits(const ipv6_address& addr, uint8_t n) {
    ipv6_address ret = addr;
    unsigned int i = 15;

    while (true) {
        if (n >= 8) {
            ret.v[i] = 0;
            n -= 8;
            i--;

            if (n == 0)
                break;
        } else {
            ret.v[i] = (uint8_t)(ret.v[i] & ~((1 << n) - 1));
            break;
        }
    }

    return ret;
}

static void handle_router_advertisement(netif& ni, packet& pack) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_router_advertisement)) {
        print("handle_router_advertisement: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_router_advertisement));
        return;
    }

    if (!valid_ndp_packet(pack))
        return;

    auto& ip = *(ipv6_header*)pack.data;
    auto& ra = *(icmpv6_router_advertisement*)(pack.data + sizeof(ipv6_header));

#if 0
    print("RA: src = {}, cur_hop_limit = {}, snac_router = {}, nd_proxy = {}, prf = {}, home_agent = {}, other_configuration = {}, managed_address_configuration = {}, router_lifetime = {}, reachable_time = {}, retrans_timer = {}\n",
          ip.src, ra.cur_hop_limit, +ra.snac_router, +ra.nd_proxy, +ra.prf,
          +ra.home_agent, +ra.other_configuration,
          +ra.managed_address_configuration, byteswap(ra.router_lifetime),
          byteswap(ra.reachable_time), byteswap(ra.retrans_timer));
#endif

    // FIXME - reachable_time (neighbour cache state machine)
    // FIXME - retrans_timer (how long to wait between neighbour solicitations)

    optional<mac_address> src_lladdr;

    auto opts = span(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_router_advertisement),
                     pack.size - sizeof(ipv6_header) - sizeof(icmpv6_router_advertisement));

    while (opts.size() >= 8) {
        auto& opt = *(ndp_option*)opts.data();

        if (opts.size() < opt.length * 8) {
            print("handle_router_advertisement: router advertisement option truncated\n");
            break;
        }

        switch (opt.type) {
            case ndp_option_type::source_lladdr: {
                auto& src = *(ndp_option_lladdr*)&opt;

#if 0
                print("source link-local address: {}\n", src.mac);
#endif

                src_lladdr = src.mac;

                break;
            }

            case ndp_option_type::prefix_info: {
                if (opts.size() * 8 < sizeof(ndp_option_prefix_info)) {
                    print("handle_router_advertisement: prefix info was {} bytes, expected {}\n",
                          opts.size() * 8, sizeof(ndp_option_prefix_info));
                    break;
                }

                auto& pref = *(ndp_option_prefix_info*)&opt;

#if 0
                print("prefix: onlink = {}, autonomous = {}, router_address = {}, dhcpv6_pd_preferred = {}, valid_lifetime = {}, preferred_lifetime = {}, prefix = {}/{}\n",
                      +pref.onlink, +pref.autonomous, +pref.router_address,
                      +pref.dhcpv6_pd_preferred, byteswap(pref.valid_lifetime),
                      byteswap(pref.preferred_lifetime), pref.prefix,
                      pref.prefix_length);
#endif

                add_ipv6_prefix(ni, zero_right_bits(pref.prefix, 128 - pref.prefix_length),
                                pref.prefix_length, byteswap(pref.valid_lifetime),
                                byteswap(pref.preferred_lifetime), pref.onlink,
                                pref.autonomous);

                break;
            }

            // FIXME - MTU
            // FIXME - DNS server

            default:
                break;
        }

        opts = span(opts.data() + (opt.length * 8), opts.size() - (opt.length * 8));
    }

    if (src_lladdr.has_value())
        add_neighbour_cache_entry(ni, *src_lladdr, ip.src, true);

    if (ra.router_lifetime == 0) {
        ipv6_router* r = nullptr;
        bool no_routers_left = false;

        {
            lock_guard lock(ni.routers_lock);

            for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                auto& r2 = containing_record<ipv6_router>(le);

                if (r2.addr == ip.src) {
                    remove_entry_list(r2.le);
                    r = &r2;
                    break;
                }
            }

            no_routers_left = is_list_empty(ni.routers);
        }

        if (r)
            delete r;

        if (no_routers_left)
            remove_ipv6_route(ni, parse_ipv6_address("::"), 0); // remove default route
    } else {
        bool router_found = false;

        {
            lock_guard lock(ni.routers_lock);

            for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                auto& r = containing_record<ipv6_router>(le);

                if (r.addr == ip.src) {
                    r.lifetime = ra.router_lifetime;
                    router_found = true;
                    break;
                }
            }
        }

        if (!router_found && src_lladdr.has_value()) {
            auto r = new ipv6_router;

            if (!r)
                print("new failed");
            else {
                r->addr = ip.src;
                r->mac = *src_lladdr;
                r->lifetime = ra.router_lifetime;

                {
                    lock_guard lock(ni.routers_lock);

                    for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                        auto& r = containing_record<ipv6_router>(le);

                        if (r.addr == ip.src) {
                            r.lifetime = ra.router_lifetime;
                            router_found = true;
                            break;
                        }
                    }

                    if (!router_found)
                        insert_tail_list(ni.routers, r->le);
                }

                if (!router_found)
                    add_ipv6_route(ni, parse_ipv6_address("::"), 0); // add default route
                else
                    delete r;
            }
        }
    }

    ni.rs_needed = false;
}

static void handle_icmpv6_error(packet& pack) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_error)) {
        print("handle_icmpv6_error: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_error));
        return;
    }

    const auto& orig_ip = *(ipv6_header*)pack.data;
    const auto& icmp = *(icmpv6_error*)(pack.data + sizeof(ipv6_header));

    if (calc_ipv6_checksum(orig_ip, span((uint8_t*)&icmp, pack.size - sizeof(ipv6_header)), IPPROTO_ICMPV6) != 0)
        return;

    if (icmp.header.next_header == IPPROTO_UDP)
        handle_icmpv6_error_udp(pack);
}

static void handle_icmpv6_packet(netif& ni, packet& pack,
                                 const mac_address& src_mac) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_header)) {
        print("handle_icmpv6_packet: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_header));
        return;
    }

    auto& header = *(icmpv6_header*)(pack.data + sizeof(ipv6_header));

#if 0
    print("icmpv6: type = {}, code = {:02x}, checksum = {:04x}\n",
          header.type, header.code, byteswap(header.checksum));
#endif

    switch (header.type) {
        case icmpv6_type::dest_unreachable:
        case icmpv6_type::packet_too_big:
        case icmpv6_type::time_exceeded:
        case icmpv6_type::param_problem:
            handle_icmpv6_error(pack);
            break;

        case icmpv6_type::echo_request:
            handle_v6_echo_request(ni, pack, src_mac);
            break;

        case icmpv6_type::router_advertisement:
            handle_router_advertisement(ni, pack);
            break;

        case icmpv6_type::neighbour_solicitation:
            handle_neighbour_solicitation(ni, pack, src_mac);
            break;

        case icmpv6_type::neighbour_advertisement:
            handle_neighbour_advertisement(ni, pack);
            break;

        default:
            break;
    }
}

static void send_to_raw_sockets(uint32_t netif_num, packet& pack) {
    auto& ip = *(ipv6_header*)pack.data;
    size_t num_sockets;

    {
        lock_guard lock(ipv6_raw_sockets_lock);
        num_sockets = num_ipv6_raw_sockets;
    }

    if (num_sockets == 0)
        return;

    auto arr = (ipv6_raw_socket**)malloc(sizeof(ipv6_raw_socket*) * num_sockets);
    if (!arr) {
        print("malloc failed\n");
        return;
    }

    size_t num = 0;

    {
        lock_guard lock(ipv6_raw_sockets_lock);

        for (auto le = ipv6_raw_sockets.next; le != &ipv6_raw_sockets; le = le->next) {
            auto& s = containing_record<ipv6_raw_socket>(le);

            arr[num] = &s;
            s.acquire();

            num++;
            if (num == num_sockets)
                break;
        }
    }

    if (num == 0) {
        free(arr);
        return;
    }

    for (size_t i = 0; i < num; i++) {
        auto& s = *arr[i];

        if (s.protocol != ip.next_header) {
            s.release();
            continue;
        }

        auto p = new ipv6_socket_packet(pack);
        if (!p) {
            print("new failed\n");
            s.release();
            continue;
        }

        p->scope_id = netif_num;

        bool done = false;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(s.buffer_lock);

            if (s.buffer_size + pack.size <= MAX_BUFFER_SIZE) {
                insert_tail_list(s.buffer, p->le);
                s.buffer_size += pack.size;
                done = true;
            }

            if (!s.signalled) {
                wake_up_waiters(s.waiters);
                s.signalled = true;
            }
        }

        if (!done) {
            delete p;
            s.release();
            continue;
        }

        s.release();
    }

    free(arr);
}

static void send_to_sockets(netif& ni, packet& pack,
                            const mac_address& src_mac) {
    auto& ip = *(ipv6_header*)pack.data;

    if (ip.next_header == IPPROTO_UDP)
        received_ipv6_udp_packet(ni, pack, src_mac);
    else if (ip.next_header == IPPROTO_TCP)
        received_ipv6_tcp_packet(ni, pack, src_mac);

    send_to_raw_sockets(ni.num, pack);
}

static bool should_accept_multicast(netif& ni, const ipv6_address& dst) {
    static const auto all_nodes = parse_ipv6_address("ff02::1");
    static const auto solicited_multicast = parse_ipv6_address("ff02::1:ff00:0");

    if (dst == all_nodes)
        return true;

    if (!memcmp(dst.v.data(), solicited_multicast.v.data(), 13)) {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            if (a.addr.v[13] == dst.v[13] && a.addr.v[14] == dst.v[14] &&
                a.addr.v[15] == dst.v[15]) {
                return true;
            }
        }
    }

    return false;
}

void handle_ipv6_packet(netif& ni, packet& pack, const mac_address& src_mac) {
    if (pack.size < sizeof(ipv6_header)) {
        print("handle_ipv6_packet: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header));
        return;
    }

    auto& header = *(ipv6_header*)pack.data;

#if 0
    print("ipv6: version = {}, traffic_class1 = {}, traffic_class2 = {}, flow_label = {}, payload_length = {}, next_header = {}, hop_limit = {}, src = {}, dst = {}\n",
          header.version, header.traffic_class1, header.traffic_class2, header.flow_label,
          byteswap(header.payload_length), header.next_header, header.hop_limit,
          header.src, header.dst);
#endif

    const auto& ip = *(ipv6_header*)pack.data;
    const auto& dst = ip.dst;

    if (is_multicast_address(ip.dst)) {
        if (!should_accept_multicast(ni, ip.dst))
            return;
    } else {
        bool found = false;

        // check if it's for us

        {
            lock_guard lock(ni.addr_lock);

            for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                auto& a = containing_record<ip_addr>(le);

                if (a.addr == dst) {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
            return;
    }

    switch ((ip_proto)header.next_header) {
        case ip_proto::icmpv6:
            handle_icmpv6_packet(ni, pack, src_mac);
            break;

        default:
            break;
    }

    send_to_sockets(ni, pack, src_mac);

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr == header.src) {
                if (c.status == neighbour_cache_status::reachable)
                    c.last_update_time = kernel_time_page.system_time;

                break;
            }
        }
    }
}

bool drop_packet(const mac_address& dst, const mac_address& us) {
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

static void reap_neighbour_cache() {
    int64_t now = kernel_time_page.system_time;

    // FIXME - implement proper RFC4861 state machine? INCOMPLETE, REACHABLE, STALE, DELAY, PROBE

    static const unsigned int REAP_TIME = 300; // 5 minutes

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            list_entry reap_list = { &reap_list, &reap_list };

            auto& ni = containing_record<netif>(le);

            {
                lock_guard lock2(ni.neighbour_lock);

                for (auto le2 = ni.neighbour_cache.next; le2 != &ni.neighbour_cache; le2 = le2->next) {
                    auto& c = containing_record<neighbour_cache_entry>(le2);

                    if (now >= c.last_update_time + (REAP_TIME * SYSTEM_TIME_RESOLUTION)) {
                        auto le_prev = le2->prev;

                        remove_entry_list(c.le);
                        insert_tail_list(reap_list, c.le);

                        le2 = le_prev;
                    }
                }
            }

            while (!is_list_empty(reap_list)) {
                auto& c = containing_record<neighbour_cache_entry>(reap_list.next);

                set_event(static_cast<event&>(c.event.get()));

                remove_entry_list(c.le);
                c.release();
            }
        }
    }
}

static void send_router_solicitation(netif& ni) {
    static const unsigned int MAX_TRIES = 3;
    static const mac_address dst_mac = { 0x33, 0x33, 0x00, 0x00, 0x00, 0x02 };

    if (ni.rs_tries == MAX_TRIES) {
        print("no response to router solicitations on {} after {} tries, giving up\n",
              ni, ni.rs_tries);
        ni.rs_needed = false;
        return;
    }

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    struct rs_pkt {
        ipv6_header ip;
        icmpv6_router_solicitation rs;
        ndp_option_lladdr opt;
    } __attribute__((packed));

    pack->size = sizeof(rs_pkt);
    auto& pkt = *(rs_pkt*)pack->data;

    memset(&pkt, 0, sizeof(pkt));

    pkt.ip.version = 6;
    pkt.ip.payload_length = byteswap((uint16_t)(sizeof(icmpv6_router_solicitation) + sizeof(ndp_option_lladdr)));
    pkt.ip.next_header = (uint8_t)ip_proto::icmpv6;
    pkt.ip.hop_limit = 255;
    pkt.ip.src = ni.ll_addr;
    pkt.ip.dst = parse_ipv6_address("ff02::2");

    pkt.rs.type = icmpv6_type::router_solicitation;

    pkt.opt.type = ndp_option_type::source_lladdr;
    pkt.opt.length = 1; // 8 bytes
    pkt.opt.mac = ni.mac;

    pkt.rs.checksum = calc_ipv6_checksum(pkt.ip, span((uint8_t*)&pkt.rs, sizeof(pkt.rs) + sizeof(pkt.opt)), IPPROTO_ICMPV6);

    auto status = ni.send_packet(dst_mac, ethertype::ipv6, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();

    ni.rs_tries++;
    ni.secs_since_last_rs = 0;
}

static void send_router_solicitations() {
    static const unsigned int RS_DELAY = 4; // seconds

    while (true) {
        netif* ni = nullptr;

        {
            lock_guard lock(netif_lock);

            for (auto le = netif_list.next; le != &netif_list; le = le->next) {
                auto& ni2 = containing_record<netif>(le);

                if (!ni2.rs_needed || ni2.rs_visited)
                    continue;

                ni2.rs_visited = true;
                ni2.secs_since_last_rs++;

                if (ni2.rs_tries == 0 || ni2.secs_since_last_rs >= RS_DELAY) {
                    ni = &ni2;
                    ni2.acquire();
                    break;
                }
            }
        }

        if (!ni)
            break;

        send_router_solicitation(*ni);
    }

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni = containing_record<netif>(le);

            ni.rs_visited = false;
        }
    }
}

static void netif_routers_tick(netif& ni) {
    while (true) {
        ipv6_router* dead_router = nullptr;
        bool no_more_routers;

        {
            lock_guard lock(ni.routers_lock);

            for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
                auto& r = containing_record<ipv6_router>(le);

                if (r.tick_visited)
                    continue;

                r.lifetime--;
                r.tick_visited = true;

                if (r.lifetime == 0) {
                    remove_entry_list(r.le);
                    dead_router = &r;
                    no_more_routers = is_list_empty(ni.routers);
                    break;
                }
            }
        }

        if (!dead_router)
            break;

        delete dead_router;

        if (no_more_routers)
            remove_ipv6_route(ni, parse_ipv6_address("::"), 64);
    }

    {
        lock_guard lock(ni.routers_lock);

        for (auto le = ni.routers.next; le != &ni.routers; le = le->next) {
            auto& r = containing_record<ipv6_router>(le);

            r.tick_visited = false;
        }
    }
}

static void netif_prefixes_tick(netif& ni) {
    netif_routers_tick(ni);

    // FIXME - also preferred_lifetime

    while (true) {
        ipv6_prefix* dead_prefix = nullptr;

        {
            lock_guard lock(ni.prefix_lock);

            for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
                auto& p = containing_record<ipv6_prefix>(le);

                if (p.tick_visited)
                    continue;

                p.valid_lifetime--;
                p.tick_visited = true;

                if (p.valid_lifetime == 0) {
                    remove_entry_list(p.le);
                    dead_prefix = &p;
                    break;
                }
            }
        }

        if (!dead_prefix)
            break;

        remove_ipv6_route(ni, dead_prefix->prefix, dead_prefix->prefix_length);

        // FIXME - should we be sending neighbour advertisements to say these IPs are going away?

        if (dead_prefix->prefix_length == 64) {
            // delete SLAAC addresses

            while (true) {
                ip_addr* dead_addr = nullptr;

                {
                    lock_guard lock(ni.addr_lock);

                    for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
                        auto& a = containing_record<ip_addr>(le);

                        if (!a.slaac)
                            continue;

                        if (!memcmp(a.addr.v.data(), dead_prefix->prefix.v.data(), 8)) {
                            remove_entry_list(a.le);
                            dead_addr = &a;
                            break;
                        }
                    }
                }

                if (!dead_addr)
                    break;

                print("{}: removing IPv6 address {}\n", ni, dead_addr->addr);
                delete dead_addr;
            }
        }

        delete dead_prefix;
    }

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            p.tick_visited = false;
        }
    }
}

static void router_lifetime_tick() {
    lock_guard lock(netif_lock);

    for (auto le = netif_list.next; le != &netif_list; le = le->next) {
        auto& ni = containing_record<netif>(le);

        netif_routers_tick(ni);
        netif_prefixes_tick(ni);
    }
}

static void network_housekeeping() {
    // once a second

    send_router_solicitations();
    router_lifetime_tick();
    reap_neighbour_cache();
}

static void network_thread() {
    auto tim_ref = create_timer(true);
    if (!tim_ref) {
        print("create_timer failed, exiting network thread\n");
        return;
    }

    auto& tim = static_cast<timer&>(tim_ref.get());

    int64_t due_time_ticks = SYSTEM_TIME_RESOLUTION; // 1 second
    auto due_time_ms = (int32_t)((due_time_ticks * 1000) / SYSTEM_TIME_RESOLUTION);

    auto status = tim.set(-due_time_ticks, nullptr, nullptr, false,
                          due_time_ms, nullptr);
    if (!success(status)) {
        print("timer::set returned {}, exiting network thread\n", status);
        return;
    }

    while (true) {
        status = wait_for_object(tim, true);
        if (!success(status)) {
            print("wait_for_object returned {}, exiting network thread\n", status);
            break;
        }

        network_housekeeping();
    }
}

bool init_net() {
    static const string_view LOOPBACK_NAME = "lo";

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

    auto lo = new loopback_netif;
    if (!lo) {
        print("new failed\n");
        return false;
    }

    auto lo_addr = new ip_addr;
    if (!lo_addr) {
        print("new failed\n");
        delete lo;
        return false;
    }

    lo->num = next_netif_num.fetch_add(1);

    memcpy(lo->name, LOOPBACK_NAME.data(), LOOPBACK_NAME.size());
    memset(lo->name + LOOPBACK_NAME.size(), 0,
           sizeof(lo->name) - LOOPBACK_NAME.size());

    memset(lo->ll_addr.v.data(), 0, lo->ll_addr.v.size());
    lo->rs_needed = false;
    lo->mtu = MTU;

    lo_addr->addr = parse_ipv6_address("::1");

    {
        lock_guard lg(lo->addr_lock);
        insert_tail_list(lo->addrs, lo_addr->le);
    }

    {
        lock_guard lg(netif_lock);
        insert_tail_list(netif_list, lo->le);
    }

    add_kernel_thread(network_thread);

    return true;
}

static nt_status netctl_socket(span<const uint8_t> buf) {
    if (buf.size() < sizeof(MOXNOX_NETCTL_SOCKET))
        return nt_status::BUFFER_TOO_SMALL;

    auto& params = *(MOXNOX_NETCTL_SOCKET*)buf.data();

    if (!params.h)
        return nt_status::INVALID_PARAMETER;

    auto t = get_current_thread();
    if (!t->proc)
        return nt_status::INTERNAL_ERROR;

    switch (params.af) {
        case AF_INET6: {
            object_ref ref;

            switch (params.type) {
                case SOCK_RAW: {
                    if (params.protocol == 0)
                        return nt_status::INVALID_PARAMETER;

                    auto s = new ipv6_raw_socket(params.protocol);
                    if (!s) {
                        print("new failed\n");
                        return nt_status::INSUFFICIENT_RESOURCES;
                    }

                    ref = object_ref{*s};

                    break;
                }

                case SOCK_DGRAM: {
                    if (params.protocol != 0 && params.protocol != IPPROTO_UDP)
                        return nt_status::INVALID_PARAMETER;

                    auto status = create_new_ipv6_udp_socket(ref);
                    if (!success(status))
                        return status;

                    break;
                }

                case SOCK_STREAM: {
                    if (params.protocol != 0 && params.protocol != IPPROTO_TCP)
                        return nt_status::INVALID_PARAMETER;

                    auto status = create_new_ipv6_tcp_socket(ref);
                    if (!success(status))
                        return status;

                    break;
                }

                default:
                    return nt_status::INVALID_PARAMETER;
            }

            HANDLE h;

            auto status = t->proc->add_handle(ref.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                                              true, h);
            if (!success(status))
                return status;

            status = t->proc->write_vm(params.h, &h, sizeof(h), nullptr);
            if (!success(status)) {
                // FIXME - close handle
                return status;
            }

            if (params.type == SOCK_RAW) {
                auto& s = static_cast<ipv6_raw_socket&>(ref.get());

                lock_guard lg(ipv6_raw_sockets_lock);
                insert_tail_list(ipv6_raw_sockets, s.le);
                num_ipv6_raw_sockets++;
                s.added_to_list = true;
            }

            break;
        }

        default:
            return nt_status::INVALID_PARAMETER;
    }

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_count(span<uint8_t> buf, uintptr_t& written) {
    if (buf.size() < sizeof(MOXNOX_NETCTL_GET_NETIF_COUNT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_COUNT*)buf.data();
    uint32_t num = 0;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            num++;
        }
    }

    out.count = num;
    written = sizeof(MOXNOX_NETCTL_GET_NETIF_COUNT);

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_ids(span<uint8_t> buf, uintptr_t& written) {
    if (buf.size() < offsetof(MOXNOX_NETCTL_GET_NETIF_IDS, id))
        return nt_status::BUFFER_TOO_SMALL;

    size_t left = (buf.size() - offsetof(MOXNOX_NETCTL_GET_NETIF_IDS, id)) / sizeof(uint32_t);

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_IDS*)buf.data();
    uint32_t num = 0;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni = containing_record<netif>(le);

            if (left == 0)
                return nt_status::BUFFER_TOO_SMALL;

            out.id[num] = ni.num;
            num++;
            left--;
        }
    }

    out.count = num;
    written = offsetof(MOXNOX_NETCTL_GET_NETIF_IDS, id) + (num * sizeof(uint32_t));

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_info(span<const uint8_t> inbuf,
                                       span<uint8_t> outbuf, uintptr_t& written) {
    if (inbuf.size() < sizeof(uint32_t))
        return nt_status::INVALID_PARAMETER;

    auto id = *(uint32_t*)inbuf.data();

    if (outbuf.size() < sizeof(MOXNOX_NETCTL_GET_NETIF_INFO))
        return nt_status::BUFFER_TOO_SMALL;

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_INFO*)outbuf.data();
    netif* ni = nullptr;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            if (ni2.num == id) {
                ni2.acquire();
                ni = &ni2;
                break;
            }
        }
    }

    if (!ni)
        return nt_status::NOT_FOUND;

    memcpy(out.name, ni->name, sizeof(ni->name));
    out.ll_addr = ni->ll_addr;

    ni->release();

    written = sizeof(MOXNOX_NETCTL_GET_NETIF_INFO);
    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_by_name(span<const uint8_t> inbuf,
                                          span<uint8_t> outbuf,
                                          uintptr_t& written) {
    if (inbuf.size() < sizeof(MOXNOX_NETCTL_GET_NETIF_BY_NAME))
        return nt_status::BUFFER_TOO_SMALL;

    if (outbuf.size() < sizeof(uint32_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto p = *(MOXNOX_NETCTL_GET_NETIF_BY_NAME*)inbuf.data();

    auto& out = *(uint32_t*)outbuf.data();

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni = containing_record<netif>(le);

            if (!memcmp(ni.name, p.name, sizeof(p.name))) {
                out = ni.num;
                written = sizeof(uint32_t);
                return nt_status::SUCCESS;
            }
        }
    }

    return nt_status::NOT_FOUND;
}

static nt_status netctl_get_netif_ip_count(span<const uint8_t> inbuf,
                                           span<uint8_t> outbuf,
                                           uintptr_t& written) {
    if (inbuf.size() < sizeof(uint32_t))
        return nt_status::INVALID_PARAMETER;

    auto id = *(uint32_t*)inbuf.data();

    if (outbuf.size() < sizeof(uint32_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto& out = *(uint32_t*)outbuf.data();
    netif* ni = nullptr;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            if (ni2.num == id) {
                ni2.acquire();
                ni = &ni2;
                break;
            }
        }
    }

    if (!ni)
        return nt_status::NOT_FOUND;

    uint32_t count = 0;

    {
        lock_guard lock(ni->addr_lock);

        for (auto le = ni->addrs.next; le != &ni->addrs; le = le->next) {
            count++;
        }
    }

    ni->release();

    out = count;
    written = sizeof(uint32_t);

    return nt_status::SUCCESS;
}

static nt_status netctl_get_netif_ips(span<const uint8_t> inbuf,
                                      span<uint8_t> outbuf,
                                      uintptr_t& written) {
    if (inbuf.size() < sizeof(uint32_t))
        return nt_status::INVALID_PARAMETER;

    auto id = *(uint32_t*)inbuf.data();

    if (outbuf.size() < offsetof(MOXNOX_NETCTL_GET_NETIF_IPS, ip))
        return nt_status::BUFFER_TOO_SMALL;

    size_t left = (outbuf.size() - offsetof(MOXNOX_NETCTL_GET_NETIF_IPS, ip)) / sizeof(uint32_t);

    auto& out = *(MOXNOX_NETCTL_GET_NETIF_IPS*)outbuf.data();
    netif* ni = nullptr;

    {
        lock_guard lock(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            if (ni2.num == id) {
                ni2.acquire();
                ni = &ni2;
                break;
            }
        }
    }

    if (!ni)
        return nt_status::NOT_FOUND;

    uint32_t num = 0;
    nt_status ret = nt_status::SUCCESS;

    {
        lock_guard lock(ni->addr_lock);

        for (auto le = ni->addrs.next; le != &ni->addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            if (left == 0) {
                ret = nt_status::BUFFER_TOO_SMALL;
                break;
            }

            out.ip[num] = a.addr;
            num++;
            left--;
        }
    }

    ni->release();

    if (!success(ret))
        return ret;

    out.count = num;
    written = offsetof(MOXNOX_NETCTL_GET_NETIF_IPS, ip) + (num * sizeof(uint32_t));

    return ret;
}

nt_status netctl::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_NETCTL_SOCKET:
            return netctl_socket(inbuf);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_COUNT:
            return netctl_get_netif_count(outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IDS:
            return netctl_get_netif_ids(outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_INFO:
            return netctl_get_netif_info(inbuf, outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME:
            return netctl_get_netif_by_name(inbuf, outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IP_COUNT:
            return netctl_get_netif_ip_count(inbuf, outbuf, written);

        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IPS:
            return netctl_get_netif_ips(inbuf, outbuf, written);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

ipv6_raw_socket::~ipv6_raw_socket() {
    {
        lock_guard lg(ipv6_raw_sockets_lock);

        if (added_to_list) {
            remove_entry_list(le);
            num_ipv6_raw_sockets--;
        }
    }

    while (true) {
        ipv6_socket_packet* pp = nullptr;

        {
            lock_guard lg(buffer_lock);

            if (is_list_empty(buffer))
                break;

            auto& p = containing_record<ipv6_socket_packet>(buffer.next);

            remove_entry_list(p.le);
            pp = &p;
        }

        if (!pp)
            break;

        delete pp;
    }
}

nt_status socket::do_recvfrom(span<const uint8_t> inbuf) {
    nt_status status;

    if (inbuf.size() < sizeof(MOXNOX_SOCKET_RECVFROM))
        return nt_status::BUFFER_TOO_SMALL;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    auto& params = *(MOXNOX_SOCKET_RECVFROM*)inbuf.data();
    size_t recv_bytes;
    int fromlen = 0;

    if (!params.buf || params.len <= 0 || !params.recv_bytes)
        return nt_status::INVALID_PARAMETER;

    if (params.fromlen) {
        uint32_t read_bytes;

        status = t.proc->read_vm(params.fromlen, &fromlen, sizeof(fromlen),
                                 &read_bytes);
        if (!success(status))
            return status;

        if (read_bytes < sizeof(fromlen))
            return nt_status::NO_MEMORY;

        if (fromlen < 0)
            return nt_status::INVALID_PARAMETER;
    }

    status = this->recvfrom(params.buf, (size_t)params.len, params.flags,
                            (sockaddr*)params.from,
                            fromlen != 0 ? &fromlen : nullptr,
                            recv_bytes);
    if (!success(status))
        return status;

    if (params.fromlen) {
        status = t.proc->write_vm(params.fromlen, &fromlen,
                                    (uint32_t)sizeof(fromlen), nullptr);
        if (!success(status))
            return status;
    }

    status = t.proc->write_vm(params.recv_bytes, &recv_bytes,
                                sizeof(size_t), nullptr);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

nt_status socket::do_sendto(span<const uint8_t> inbuf) {
    nt_status status;

    if (inbuf.size() < sizeof(MOXNOX_SOCKET_SENDTO))
        return nt_status::BUFFER_TOO_SMALL;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    auto& params = *(MOXNOX_SOCKET_SENDTO*)inbuf.data();
    sockaddr* to = nullptr;
    size_t sent_bytes;

    if ((!params.buf && params.len > 0) || params.len < 0 || !params.sent_bytes)
        return nt_status::INVALID_PARAMETER;

    if ((size_t)params.len > PAGE_SIZE)
        params.len = PAGE_SIZE;

    if ((params.tolen == 0 && params.to) || (params.tolen != 0 && !params.to) || params.tolen < 0)
        return nt_status::INVALID_PARAMETER;

    if (params.to) {
        uint32_t read_bytes;

        if ((size_t)params.tolen > PAGE_SIZE)
            params.tolen = PAGE_SIZE;

        to = (sockaddr*)malloc((size_t)params.tolen);
        if (!to) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        status = t.proc->read_vm(params.to, to, (uint32_t)params.tolen,
                                 &read_bytes);
        if (!success(status)) {
            free(to);
            return status;
        }

        if (read_bytes < (uint32_t)params.tolen) {
            free(to);
            return nt_status::NO_MEMORY;
        }
    }

    status = this->sendto(params.buf, (size_t)params.len, params.flags,
                          to, params.tolen, sent_bytes);
    if (!success(status)) {
        if (to)
            free(to);

        return status;
    }

    if (to)
        free(to);

    status = t.proc->write_vm(params.sent_bytes, &sent_bytes, sizeof(size_t),
                              nullptr);
    if (!success(status))
        return status;

    return nt_status::SUCCESS;
}

nt_status socket::do_bind(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    return this->bind((sockaddr*)inbuf.data(), inbuf.size());
}

nt_status socket::do_connect(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    return this->connect((sockaddr*)inbuf.data(), inbuf.size());
}

nt_status socket::do_listen(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(int))
        return nt_status::BUFFER_TOO_SMALL;

    auto backlog = *(int*)inbuf.data();

    return this->listen(backlog);
}

nt_status socket::do_accept(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                            uintptr_t& written_bytes) {
    if (inbuf.size() < sizeof(MOXNOX_SOCKET_ACCEPT_IN))
        return nt_status::BUFFER_TOO_SMALL;

    if (outbuf.size() < sizeof(MOXNOX_SOCKET_ACCEPT_OUT))
        return nt_status::BUFFER_TOO_SMALL;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    auto& params_in = *(MOXNOX_SOCKET_ACCEPT_IN*)inbuf.data();
    auto& params_out = *(MOXNOX_SOCKET_ACCEPT_OUT*)outbuf.data();

    HANDLE sock;
    int addrlen = params_in.addrlen;

    auto status = this->accept(sock, params_in.addr, addrlen);

    if (!success(status))
        return status;

    params_out.sock = sock;
    params_out.addrlen = addrlen;

    written_bytes = sizeof(MOXNOX_SOCKET_ACCEPT_OUT);

    return nt_status::SUCCESS;
}

nt_status socket::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t> outbuf, uintptr_t& written_bytes) {
    switch (code) {
        case IOCTL_MOXNOX_SOCKET_RECVFROM:
            return do_recvfrom(inbuf);

        case IOCTL_MOXNOX_SOCKET_SENDTO:
            return do_sendto(inbuf);

        case IOCTL_MOXNOX_SOCKET_BIND:
            return do_bind(inbuf);

        case IOCTL_MOXNOX_SOCKET_CONNECT:
            return do_connect(inbuf);

        case IOCTL_MOXNOX_SOCKET_LISTEN:
            return do_listen(inbuf);

        case IOCTL_MOXNOX_SOCKET_ACCEPT:
            return do_accept(inbuf, outbuf, written_bytes);

        default:
            return nt_status::INVALID_PARAMETER;
    }
}

nt_status ipv6_raw_socket::recvfrom(char* us_buf, size_t len, int flags,
                                    sockaddr* us_from, int* fromlen,
                                    size_t& recv_bytes) {
    // FIXME - MSG_TRUNC
    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    auto& t = *get_current_thread();

    acquire();

    while (true) {
        auto status = wait_for_object(*this, true);
        if (!success(status)) {
            release();
            return status;
        }

        if (dead) {
            release();
            return nt_status::CONNECTION_DISCONNECTED;
        }

        ipv6_socket_packet* sp = nullptr;
        nt_status error = nt_status::SUCCESS;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(buffer_lock);

            if (!success(pending_error)) {
                error = pending_error;
                pending_error = nt_status::SUCCESS;

                if (is_list_empty(buffer))
                    signalled = false;
            }

            if (!is_list_empty(buffer)) {
                sp = &containing_record<ipv6_socket_packet>(buffer.next);
                remove_entry_list(sp->le);

                if (is_list_empty(buffer))
                    signalled = false;
            }
        }

        if (!success(error)) {
            release();
            return error;
        }

        if (!sp)
            continue;

        auto to_copy = (uint32_t)min(sp->pack.size - sizeof(ipv6_header), len);

        status = t.proc->write_vm(us_buf, sp->pack.data + sizeof(ipv6_header),
                                  to_copy, nullptr);
        if (!success(status)) {
            delete sp;
            release();
            return status;
        }

        recv_bytes = to_copy;

        if (us_from && fromlen && *fromlen > 0) {
            sockaddr_in6 addr;

            auto& ip = *(ipv6_header*)sp->pack.data;

            memset(&addr, 0, sizeof(addr));

            addr.sin6_family = AF_INET6;
            addr.sin6_port = 0;
            addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does
            memcpy(addr.sin6_addr, ip.src.v.data(), sizeof(ipv6_address));
            addr.sin6_scope_id = sp->scope_id;

            status = t.proc->write_vm(us_from, &addr,
                                      (uint32_t)min((size_t)*fromlen, sizeof(addr)),
                                      nullptr);
            if (!success(status)) {
                delete sp;
                release();
                return status;
            }

            *fromlen = sizeof(addr);
        }

        delete sp;

        break;
    }

    release();

    return nt_status::SUCCESS;
}

static bool is_link_local(const ipv6_address& addr) {
    // link-local unicast
    if (addr.v[0] == 0xfe && (addr.v[1] & 0xc0) == 0x80)
        return true;

    // interface-local multicast or link-local multicast
    if (addr.v[0] == 0xff && (addr.v[1] == 0x01 || addr.v[1] == 0x02))
        return true;

    return false;
}

static bool ipv6_prefix_match(const ipv6_address& addr, const ipv6_address& pref,
                              uint8_t len) {
    auto addr2 = zero_right_bits(addr, 128 - len);

    return addr2 == pref;
}

static netif* lookup_route(const ipv6_address& dst) {
    lock_guard lock(ipv6_routes_lock);

    ipv6_route* best_match = nullptr;

    for (auto le = ipv6_routes.next; le != &ipv6_routes; le = le->next) {
        auto& r = containing_record<ipv6_route>(le);

        if (!ipv6_prefix_match(dst, r.prefix, r.prefix_length))
            continue;

        if (!best_match || (r.prefix_length > best_match->prefix_length))
            best_match = &r;
    }

    if (!best_match)
        return nullptr;

    best_match->ni.acquire();

    return &best_match->ni;
}

static uint8_t ipv6_match_len(const ipv6_address& addr1, const ipv6_address& addr2) {
    uint8_t ret = 0;

    for (uint8_t i = 0; i < 16; i++) {
        if (addr1.v[i] == addr2.v[i])
            ret += 8;
        else {
            ret += (uint8_t)countl_zero((uint8_t)(addr1.v[i] ^ addr2.v[i]));
            break;
        }
    }

    return ret;
}

nt_status ipv6_choose_interface(const ipv6_address& dst, uint32_t scope_id,
                                netif*& ni, ipv6_address& src) {
    // see RFC6724

    // bind overrides everything
    if (src != parse_ipv6_address("::")) {
        lock_guard lg(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            {
                lock_guard lock(ni2.addr_lock);

                for (auto le2 = ni2.addrs.next; le2 != &ni2.addrs; le2 = le2->next) {
                    auto& a = containing_record<ip_addr>(le2);

                    if (a.addr == src) {
                        ni2.acquire();

                        ni = &ni2;
                        return nt_status::SUCCESS;
                    }
                }
            }
        }

        return nt_status::NETWORK_UNREACHABLE;
    }

    // rule 1: prefer same address

    {
        lock_guard lg(netif_lock);

        for (auto le = netif_list.next; le != &netif_list; le = le->next) {
            auto& ni2 = containing_record<netif>(le);

            {
                lock_guard lock(ni2.addr_lock);

                for (auto le2 = ni2.addrs.next; le2 != &ni2.addrs; le2 = le2->next) {
                    auto& a = containing_record<ip_addr>(le2);

                    if (a.addr == dst) {
                        ni2.acquire();

                        ni = &ni2;
                        src = dst;
                        return nt_status::SUCCESS;
                    }
                }
            }
        }
    }

    // rule 2: if link-local, choose link-local

    if (is_link_local(dst)) {
        if (scope_id == 0)
            return nt_status::NETWORK_UNREACHABLE;

        ni = nullptr;

        {
            lock_guard lg(netif_lock);

            for (auto le = netif_list.next; le != &netif_list; le = le->next) {
                auto& ni2 = containing_record<netif>(le);

                if (ni2.num == scope_id) {
                    ni = &ni2;
                    ni2.acquire();
                    break;
                }
            }
        }

        if (!ni)
            return nt_status::NETWORK_UNREACHABLE;

        src = ni->ll_addr;
        return nt_status::SUCCESS;
    }

    ni = lookup_route(dst);
    if (!ni)
        return nt_status::HOST_UNREACHABLE;

    // FIXME - consult routing tables

    // FIXME - rule 3: avoid deprecated address
    // rule 4: prefer home addresses (Mobile IPv6, not relevant)
    // FIXME - rule 5: prefer outgoing interface
    // FIXME - rule 6: prefer matching label
    // FIXME - rule 7: prefer temporary addresses

    // rule 8: prefer longest matching prefix

    auto& n = *ni;

    {
        lock_guard lock(n.addr_lock);

        ip_addr* best = nullptr;
        uint8_t best_match_len = 0;

        for (auto le = n.addrs.next; le != &n.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);
            auto match_len = ipv6_match_len(dst, a.addr);

            if (!best || match_len > best_match_len) {
                best = &a;
                best_match_len = match_len;
            }
        }

        if (!best) {
            ni->release();
            return nt_status::NETWORK_UNREACHABLE;
        }

        src = best->addr;
        return nt_status::SUCCESS;
    }
}

static void send_neighbour_solicitation(netif& ni, const ipv6_address& dst) {
    struct ns_pkt {
        ipv6_header ip;
        icmpv6_neighbour_solicitation ns;
        ndp_option_lladdr opt;
    } __attribute__((packed));

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& pkt = *(ns_pkt*)pack->data;
    pack->size = sizeof(pkt);

    memset(&pkt, 0, sizeof(pkt));

    mac_address dst_mac;

    dst_mac.v[0] = 0x33;
    dst_mac.v[1] = 0x33;
    dst_mac.v[2] = 0xff;
    dst_mac.v[3] = dst.v[13];
    dst_mac.v[4] = dst.v[14];
    dst_mac.v[5] = dst.v[15];

    pkt.ip.version = 6;
    pkt.ip.payload_length = byteswap((uint16_t)(sizeof(icmpv6_neighbour_solicitation) + sizeof(ndp_option_lladdr)));
    pkt.ip.next_header = (uint8_t)ip_proto::icmpv6;
    pkt.ip.hop_limit = 255;
    pkt.ip.src = ni.ll_addr;

    pkt.ip.dst.v[0] = 0xff;
    pkt.ip.dst.v[1] = 0x2;
    pkt.ip.dst.v[2] = 0x0;
    pkt.ip.dst.v[3] = 0x0;
    pkt.ip.dst.v[4] = 0x0;
    pkt.ip.dst.v[5] = 0x0;
    pkt.ip.dst.v[6] = 0x0;
    pkt.ip.dst.v[7] = 0x0;
    pkt.ip.dst.v[8] = 0x0;
    pkt.ip.dst.v[9] = 0x0;
    pkt.ip.dst.v[10] = 0x0;
    pkt.ip.dst.v[11] = 0x1;
    pkt.ip.dst.v[12] = 0xff;
    pkt.ip.dst.v[13] = dst.v[13];
    pkt.ip.dst.v[14] = dst.v[14];
    pkt.ip.dst.v[15] = dst.v[15];

    pkt.ns.type = icmpv6_type::neighbour_solicitation;
    pkt.ns.target = dst;

    pkt.opt.type = ndp_option_type::source_lladdr;
    pkt.opt.length = 1; // 8 bytes
    pkt.opt.mac = ni.mac;

    pkt.ns.checksum = calc_ipv6_checksum(pkt.ip, span((uint8_t*)&pkt.ns, sizeof(pkt.ns) + sizeof(pkt.opt)), IPPROTO_ICMPV6);

    auto status = ni.send_packet(dst_mac, ethertype::ipv6, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();
}

static mac_address search_neighbour_cache(netif& ni, const ipv6_address& dst) {
    neighbour_cache_entry* nce = nullptr;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr == dst) {
                if (c.status == neighbour_cache_status::reachable)
                    return c.mac;

                c.acquire();
                nce = &c;
                break;
            }
        }
    }

    if (nce) { // found, but incomplete
        wait_for_object(nce->event.get(), false);

        if (nce->status == neighbour_cache_status::reachable) {
            auto ret = nce->mac;

            nce->release();
            return ret;
        } else
            nce->release();
    }

    nce = new neighbour_cache_entry(ni);
    if (!nce) {
        print("new failed\n");
        return NULL_MAC_ADDRESS;
    }

    neighbour_cache_entry* nce2 = nullptr;

    nce->addr = dst;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return NULL_MAC_ADDRESS;
    }

    nce->status = neighbour_cache_status::incomplete;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr == dst) {
                c.acquire();
                nce2 = &c;
                break;
            }
        }

        if (!nce2) {
            insert_tail_list(ni.neighbour_cache, nce->le);
            nce->acquire();
        }
    }

    if (nce2) { // race and we lost
        nce->release();

        wait_for_object(nce2->event.get(), false);

        if (nce2->status == neighbour_cache_status::reachable) {
            auto ret = nce2->mac;

            nce2->release();
            return ret;
        } else {
            nce2->release();
            return NULL_MAC_ADDRESS;
        }
    }

    for (unsigned int i = 0; i < 3; i++) {
        uint64_t timeout = SYSTEM_TIME_RESOLUTION; // 1 second

        send_neighbour_solicitation(ni, dst);

        auto status = wait_for_object(nce->event.get(), false, timeout);

        if (status == nt_status::TIMEOUT)
            continue;
        else if (!success(status)) {
            set_event(static_cast<event&>(nce->event.get()));

            {
                lock_guard lock(ni.neighbour_lock);

                if (nce->status != neighbour_cache_status::reachable) {
                    remove_entry_list(nce->le);
                    nce->release();
                }
            }

            nce->release();

            print("wait_for_object returned {}\n", status);
            return NULL_MAC_ADDRESS;
        }

        if (nce->status == neighbour_cache_status::reachable) {
            auto ret = nce->mac;

            nce->release();
            return ret;
        }
    }

    set_event(static_cast<event&>(nce->event.get()));

    auto ret = NULL_MAC_ADDRESS;

    {
        lock_guard lock(ni.neighbour_lock);

        if (nce->status == neighbour_cache_status::reachable)
            ret = nce->mac;
        else {
            remove_entry_list(nce->le);
            nce->release();
        }
    }

    nce->release();

    return ret;
}

mac_address find_mac_address(netif& ni, const ipv6_address& dst) {
    if (dst.v[0] == 0xff) { // multicast
        mac_address ret;

        ret.v[0] = 0x33;
        ret.v[1] = 0x33;
        ret.v[2] = dst.v[12];
        ret.v[3] = dst.v[13];
        ret.v[4] = dst.v[14];
        ret.v[5] = dst.v[15];

        return ret;
    }

    if (is_link_local(dst))
        return search_neighbour_cache(ni, dst);

    bool on_link = false;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            if (ipv6_prefix_match(dst, p.prefix, p.prefix_length)) {
                on_link = p.on_link;
                break;
            }
        }
    }

    if (on_link)
        return search_neighbour_cache(ni, dst);
    else {
        lock_guard lock(ni.routers_lock);

        if (is_list_empty(ni.routers))
            return NULL_MAC_ADDRESS;

        auto& r = containing_record<ipv6_router>(ni.routers.next);

        return r.mac;
    }
}

void maybe_add_to_neighbour_cache(netif& ni, const ipv6_address& dst,
                                  const mac_address& mac) {
    bool on_link = false;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p = containing_record<ipv6_prefix>(le);

            if (ipv6_prefix_match(dst, p.prefix, p.prefix_length)) {
                on_link = p.on_link;
                break;
            }
        }
    }

    if (!on_link)
        return;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr == dst) {
                if (c.status != neighbour_cache_status::reachable) {
                    c.mac = mac;
                    set_event(static_cast<event&>(c.event.get()));
                }
                return;
            }
        }
    }

    auto nce = new neighbour_cache_entry(ni);
    if (!nce) {
        print("new failed\n");
        return;
    }

    nce->addr = dst;
    nce->mac = mac;

    nce->event = create_event(false);
    if (!nce->event) {
        print("create_event failed\n");
        nce->release();
        return;
    }

    set_event(static_cast<event&>(nce->event.get()));

    nce->status = neighbour_cache_status::reachable;

    bool found = false;

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr == dst) {
                if (c.status != neighbour_cache_status::reachable) {
                    c.mac = mac;
                    set_event(static_cast<event&>(c.event.get()));
                }

                found = true;
                break;
            }
        }

        if (!found) {
            insert_tail_list(ni.neighbour_cache, nce->le);
            nce->acquire();
        }
    }

    nce->release();
}

void send_address_unreachable(netif& ni, const packet& pack,
                              const ipv6_address& addr) {
    static const size_t MIN_MTU = 1280;

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    size_t payload_size = min(pack.size, MIN_MTU - sizeof(ipv6_header) - sizeof(icmpv6_error));

    pack2->size = sizeof(ipv6_header) + offsetof(icmpv6_error, header) + payload_size;

    auto& ip = *(ipv6_header*)pack2->data;
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label = 0;
    ip.payload_length = byteswap((uint16_t)(offsetof(icmpv6_error, header) + payload_size));
    ip.next_header = IPPROTO_ICMPV6;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = addr;
    ip.dst = addr;

    auto& err = *(icmpv6_error*)(pack2->data + sizeof(ipv6_header));

    err.type = icmpv6_type::dest_unreachable;
    err.code = (uint8_t)icmpv6_dest_unreachable_code::address_unreachable;
    err.checksum = 0;
    err.reserved = 0;

    memcpy(&err.header, pack.data, payload_size);

    err.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&err, offsetof(icmpv6_error, header) + payload_size), IPPROTO_ICMPV6);

    handle_ipv6_packet(ni, *pack2, ni.mac);

    pack2->release();
}

nt_status dispatch_packet(netif& ni, packet& pack, const mac_address& dst_mac) {
    nt_status status;

    auto& ip = *(ipv6_header*)pack.data;

    if (ip.src == ip.dst)
        handle_ipv6_packet(ni, pack, ni.mac);
    else {
        if (is_multicast_address(ip.dst) && should_accept_multicast(ni, ip.dst))
            handle_ipv6_packet(ni, pack, ni.mac); // send copy to ourselves

        status = ni.send_packet(dst_mac, ethertype::ipv6, pack);
        if (!success(status)) {
            print("netif::send_packet returned {}\n", status);
            return status;
        }
    }

    return nt_status::SUCCESS;
}

nt_status ipv6_raw_socket::sendto(const char* buf, size_t len, int flags,
                                  sockaddr* to, int tolen, size_t& sent_bytes) {
    nt_status status;
    uint32_t read_bytes;
    ipv6_address src;

    if (!to || tolen < 0 || (size_t)tolen < sizeof(sockaddr_in6))
        return nt_status::INVALID_PARAMETER;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    auto us_buf = span(buf, len);
    auto& to6 = *(sockaddr_in6*)to;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    {
        lock_guard lock(bind_lock);

        if (bind_done)
            src = bound_address;
        else
            src = parse_ipv6_address("::");
    }

    auto& dst = *(ipv6_address*)to6.sin6_addr;

    netif* pni;

    status = ipv6_choose_interface(dst, to6.sin6_scope_id, pni, src);
    if (!success(status))
        return status;

    auto& ni = *pni;

    if (us_buf.size() + sizeof(ipv6_header) > ni.mtu) {
        ni.release();
        return nt_status::INVALID_BUFFER_SIZE;
    }

    mac_address mac;

    if (src == dst)
        mac = ni.mac;
    else
        mac = find_mac_address(ni, dst);

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    pack->size = sizeof(ipv6_header) + us_buf.size();

    auto& ip = *(ipv6_header*)pack->data;
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label = 0;
    ip.payload_length = byteswap((uint16_t)us_buf.size());
    ip.next_header = (uint8_t)protocol;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = src;
    ip.dst = dst;

    status = t.proc->read_vm(us_buf.data(), pack->data + sizeof(ipv6_header),
                             (uint32_t)us_buf.size(), &read_bytes);

    if (success(status) && read_bytes < us_buf.size())
        status = nt_status::NO_MEMORY;

    if (!success(status)) {
        pack->release();
        ni.release();
        return status;
    }

    // FIXME - should only be if IPV6_CHECKSUM sockopt set
    if (protocol == IPPROTO_ICMPV6 && us_buf.size() >= sizeof(icmpv6_header)) {
        auto& icmp = *(icmpv6_header*)(pack->data + sizeof(ipv6_header));

        icmp.checksum = 0;
        icmp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&icmp, us_buf.size()), IPPROTO_ICMPV6);
    }

    if (mac == NULL_MAC_ADDRESS) {
        send_address_unreachable(ni, *pack, src);
        pack->release();
        ni.release();
        return nt_status::HOST_UNREACHABLE;
    }

    status = dispatch_packet(ni, *pack, mac);
    if (!success(status)) {
        pack->release();
        ni.release();
        return status;
    }

    pack->release();
    ni.release();

    sent_bytes = us_buf.size();

    return status;
}

netif::~netif() {
    // not routes or neighbour cache, as they hold reference to netif

    while (true) {
        ip_addr* a = nullptr;

        {
            lock_guard lock(addr_lock);

            if (!is_list_empty(addrs)) {
                a = &containing_record<ip_addr>(addrs.next);
                remove_entry_list(a->le);
            }
        }

        if (!a)
            break;

        delete a;
    }

    while (true) {
        ipv6_prefix* pref = nullptr;

        {
            lock_guard lock(prefix_lock);

            if (!is_list_empty(prefixes)) {
                pref = &containing_record<ipv6_prefix>(prefixes.next);
                remove_entry_list(pref->le);
            }
        }

        if (!pref)
            break;

        delete pref;
    }

    while (true) {
        ipv6_router* r = nullptr;

        {
            lock_guard lock(routers_lock);

            if (!is_list_empty(routers)) {
                r = &containing_record<ipv6_router>(routers.next);
                remove_entry_list(r->le);
            }
        }

        if (!r)
            break;

        delete r;
    }
}

nt_status loopback_netif::send_packet(const mac_address&, ethertype, packet&) {
    // can happen by doing e.g. "ping ff02::1%lo"

    // silently drop

    return nt_status::SUCCESS;
}

bool is_our_ip(const ipv6_address& ip) {
    lock_guard lock(netif_lock);

    for (auto le = netif_list.next; le != &netif_list; le = le->next) {
        auto& ni = containing_record<netif>(le);

        {
            lock_guard lock2(ni.addr_lock);

            for (auto le2 = ni.addrs.next; le2 != &ni.addrs; le2 = le2->next) {
                auto& a = containing_record<ip_addr>(le2);

                if (a.addr == ip)
                    return true;
            }
        }
    }

    return false;
}

nt_status ipv6_raw_socket::bind(const sockaddr* addr, size_t addrlen) {
    if (addr->sa_family != AF_INET6)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(sockaddr_in6))
        return nt_status::BUFFER_TOO_SMALL;

    auto& b = *(sockaddr_in6*)addr;

    auto ip = *(ipv6_address*)b.sin6_addr;

    if (ip != parse_ipv6_address("::")) {
        if (!is_our_ip(ip))
            return nt_status::INVALID_ADDRESS_COMPONENT;
    }

    {
        lock_guard lock(bind_lock);

        if (bind_done)
            return nt_status::INVALID_PARAMETER;

        bound_address = ip;

        bind_done = true;
    }

    return nt_status::SUCCESS;
}

void ipv6_raw_socket::last_handle_closed() {
    dead = true;

    {
        sync_lock sl;
        lock_guard lock(sl);

        if (!signalled) {
            wake_up_waiters(waiters);
            signalled = true;
        }
    }
}

ipv6_address find_ll_address(const mac_address& m) {
    ipv6_address ret;

    ret.v[0] = 0xfe;
    ret.v[1] = 0x80;
    ret.v[2] = 0x00;
    ret.v[3] = 0x00;
    ret.v[4] = 0x00;
    ret.v[5] = 0x00;
    ret.v[6] = 0x00;
    ret.v[7] = 0x00;
    ret.v[8] = m.v[0] ^ 0x02;
    ret.v[9] = m.v[1];
    ret.v[10] = m.v[2];
    ret.v[11] = 0xff;
    ret.v[12] = 0xfe;
    ret.v[13] = m.v[3];
    ret.v[14] = m.v[4];
    ret.v[15] = m.v[5];

    return ret;
}
