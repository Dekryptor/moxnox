module;

#include <array>
#include <span>
#include <mutex>
#include <bit>
#include <atomic>
#include <string_view>
#include <stdint.h>

module net;

import bsod;
import print;
import object_ref;
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

static const uint8_t DEFAULT_HOP_LIMIT = 64;
static const size_t MAX_BUFFER_SIZE = 1048576;
static const mac_address NULL_MAC_ADDRESS = { 0, 0, 0, 0, 0, 0 };

static object_ref netctl_obj;

static spinlock ipv6_sockets_lock;
static list_entry ipv6_sockets = { &ipv6_sockets, &ipv6_sockets };
static size_t num_ipv6_sockets = 0;

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
};

static spinlock ipv6_routes_lock;
static list_entry ipv6_routes = { &ipv6_routes, &ipv6_routes };

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

    pkt.na.checksum = calc_icmpv6_checksum(pkt.ip, span((uint8_t*)&pkt.na, sizeof(pkt.na) + sizeof(pkt.opt)));

    auto status = ni.send_packet(dst_mac, ethertype::ipv6, *pack);
    if (!success(status))
        print("send_packet returned {}\n", status);

    pack->release();
}

static void handle_neighbour_solicitation(netif& ni, packet& pack,
                                          const mac_address& src_mac) {
    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation)) {
        print("handle_neighbour_solicitation: buffer length was {}, expected at least {}\n",
              pack.size, sizeof(ipv6_header) + sizeof(icmpv6_neighbour_solicitation));
        return;
    }

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

static bool is_multicast_address(const ipv6_address& addr) {
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

    header.checksum = calc_icmpv6_checksum(ip, span((uint8_t*)&header, pack2->size - sizeof(ipv6_header)));

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

static void add_ipv6_prefix(netif& ni, const ipv6_address& prefix,
                            uint8_t prefix_length, uint32_t valid_lifetime,
                            uint32_t preferred_lifetime) {
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
                return;
            }
        }
    }

    if (found) {
        delete found;
        remove_ipv6_route(ni, prefix, prefix_length);
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

    bool beaten_to_it = false;

    {
        lock_guard lock(ni.prefix_lock);

        for (auto le = ni.prefixes.next; le != &ni.prefixes; le = le->next) {
            auto& p2 = containing_record<ipv6_prefix>(le);

            if (p2.prefix == prefix && p2.prefix_length == prefix_length) {
                p2.valid_lifetime = valid_lifetime;
                p2.preferred_lifetime = preferred_lifetime;
                beaten_to_it = true;
                break;
            }
        }

        if (!beaten_to_it)
            insert_tail_list(ni.prefixes, p->le);
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

    auto& ip = *(ipv6_header*)pack.data;
    auto& ra = *(icmpv6_router_advertisement*)(pack.data + sizeof(ipv6_header));

    print("FIXME - handle_router_advertisement\n"); // FIXME

    print("RA: src = {}, cur_hop_limit = {}, snac_router = {}, nd_proxy = {}, prf = {}, home_agent = {}, other_configuration = {}, managed_address_configuration = {}, router_lifetime = {}, reachable_time = {}, retrans_timer = {}\n",
          ip.src, ra.cur_hop_limit, +ra.snac_router, +ra.nd_proxy, +ra.prf,
          +ra.home_agent, +ra.other_configuration,
          +ra.managed_address_configuration, byteswap(ra.router_lifetime),
          byteswap(ra.reachable_time), byteswap(ra.retrans_timer));

    // FIXME - reachable_time (neighbour cache state machine)
    // FIXME - retrans_timer (how long to wait between neighbour solicitations)

    optional<mac_address> src_lladdr;

    auto opts = span(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_router_advertisement),
                     pack.size - sizeof(ipv6_header) - sizeof(icmpv6_router_advertisement));

    while (opts.size() >= 8) {
        auto& opt = *(ndp_option*)opts.data();

        if (opts.size() < opt.length * 8) {
            print("router advertisement option truncated\n");
            break;
        }

        switch (opt.type) {
            case ndp_option_type::source_lladdr: {
                auto& src = *(ndp_option_lladdr*)&opt;

                print("source link-local address: {}\n", src.mac);

                src_lladdr = src.mac;

                break;
            }

            case ndp_option_type::prefix_info: {
                if (opts.size() * 8 < sizeof(ndp_option_prefix_info)) {
                    print("prefix info was {} bytes, expected {}\n",
                          opts.size() * 8, sizeof(ndp_option_prefix_info));
                    break;
                }

                auto& pref = *(ndp_option_prefix_info*)&opt;

                print("prefix: onlink = {}, autonomous = {}, router_address = {}, dhcpv6_pd_preferred = {}, valid_lifetime = {}, preferred_lifetime = {}, prefix = {}/{}\n",
                      +pref.onlink, +pref.autonomous, +pref.router_address,
                      +pref.dhcpv6_pd_preferred, byteswap(pref.valid_lifetime),
                      byteswap(pref.preferred_lifetime), pref.prefix,
                      pref.prefix_length);

                add_ipv6_prefix(ni, zero_right_bits(pref.prefix, 128 - pref.prefix_length),
                                pref.prefix_length, byteswap(pref.valid_lifetime),
                                byteswap(pref.preferred_lifetime));

                // FIXME - if on-link flag set, add prefix as on-link
                // FIXME - if autonomous flag set and length is 64, add as SLAAC address

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

    // FIXME - router_lifetime (how long default route is valid for)

    // FIXME - router solicitation no longer needed
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
        case icmpv6_type::neighbour_solicitation:
            handle_neighbour_solicitation(ni, pack, src_mac);
            break;

        case icmpv6_type::neighbour_advertisement:
            handle_neighbour_advertisement(ni, pack);
            break;

        case icmpv6_type::router_advertisement:
            handle_router_advertisement(ni, pack);
            break;

        case icmpv6_type::echo_request:
            handle_v6_echo_request(ni, pack, src_mac);
            break;

        default:
            break;
    }
}

static void send_to_sockets(uint32_t netif_num, packet& pack) {
    auto& ip = *(ipv6_header*)pack.data;

    // FIXME - SOCK_STREAM sockets will be different
    // FIXME - UDP

    size_t num_sockets;

    {
        lock_guard lock(ipv6_sockets_lock);
        num_sockets = num_ipv6_sockets;
    }

    if (num_sockets == 0)
        return;

    auto arr = (ipv6_socket**)malloc(sizeof(ipv6_socket*) * num_sockets);
    if (!arr) {
        print("malloc failed\n");
        return;
    }

    size_t num = 0;

    {
        lock_guard lock(ipv6_sockets_lock);

        for (auto le = ipv6_sockets.next; le != &ipv6_sockets; le = le->next) {
            auto& s = containing_record<ipv6_socket>(le);

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

        if (s.type != SOCK_RAW) {
            s.release();
            continue;
        }

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

    send_to_sockets(ni.num, pack);

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

    pkt.rs.checksum = calc_icmpv6_checksum(pkt.ip, span((uint8_t*)&pkt.rs, sizeof(pkt.rs) + sizeof(pkt.opt)));

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

static void network_housekeeping() {
    // once a second

    send_router_solicitations();
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
            // FIXME - UDP (SOCK_DGRAM)
            // FIXME - TCP (SOCK_STREAM)

            if (params.type != SOCK_RAW)
                return nt_status::INVALID_PARAMETER;

            if (params.protocol == 0)
                return nt_status::INVALID_PARAMETER;

            auto s = new ipv6_socket(params.type, params.protocol);
            if (!s) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            object_ref ref{*s};
            HANDLE h;

            auto status = t->proc->add_handle(*s, 0, true, h);
            if (!success(status))
                return status;

            status = t->proc->write_vm(params.h, &h, sizeof(h), nullptr);
            if (!success(status)) {
                // FIXME - close handle
                return status;
            }

            {
                lock_guard lg(ipv6_sockets_lock);
                insert_tail_list(ipv6_sockets, s->le);
                num_ipv6_sockets++;
                s->added_to_list = true;
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
    *(ipv6_address*)out.ll_addr = ni->ll_addr;

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

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}

ipv6_socket::~ipv6_socket() {
    {
        lock_guard lg(ipv6_sockets_lock);

        if (added_to_list) {
            remove_entry_list(le);
            num_ipv6_sockets--;
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
        status = t.proc->read_vm(params.fromlen, &fromlen,
                                    sizeof(fromlen), nullptr);
        if (!success(status))
            return status;

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

    if (!params.buf || params.len <= 0 || !params.sent_bytes)
        return nt_status::INVALID_PARAMETER;

    if ((size_t)params.len > PAGE_SIZE)
        params.len = PAGE_SIZE;

    if ((params.tolen == 0 && params.to) || (params.tolen != 0 && !params.to) || params.tolen < 0)
        return nt_status::INVALID_PARAMETER;

    if (params.to) {
        if ((size_t)params.tolen > PAGE_SIZE)
            params.tolen = PAGE_SIZE;

        to = (sockaddr*)malloc((size_t)params.tolen);
        if (!to) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        status = t.proc->read_vm(params.to, to, (uint32_t)params.tolen, nullptr);
        if (!success(status)) {
            free(to);
            return status;
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

nt_status socket::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t>, uintptr_t&) {
    switch (code) {
        case IOCTL_MOXNOX_SOCKET_RECVFROM:
            return do_recvfrom(inbuf);

        case IOCTL_MOXNOX_SOCKET_SENDTO:
            return do_sendto(inbuf);

        default:
            return nt_status::INVALID_PARAMETER;
    }
}

nt_status ipv6_socket::recvfrom(char* us_buf, size_t len, int flags,
                                sockaddr* us_from, int* fromlen,
                                size_t& recv_bytes) {
    // FIXME - MSG_TRUNC
    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    sockaddr_in6 addr;
    auto& t = *get_current_thread();

    acquire();

    while (true) {
        // FIXME - break lock if last handle to socket closed
        auto status = wait_for_object(*this, true);
        if (!success(status)) {
            release();
            return status;
        }

        ipv6_socket_packet* sp = nullptr;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(buffer_lock);

            if (!is_list_empty(buffer)) {
                sp = &containing_record<ipv6_socket_packet>(buffer.next);
                remove_entry_list(sp->le);

                if (is_list_empty(buffer))
                    signalled = false;
            }
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
            auto& ip = *(ipv6_header*)sp->pack.data;

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

nt_status ipv6_socket::sendto(const char* us_buf, size_t len, int flags,
                              sockaddr* to, int tolen, size_t& sent_bytes) {
    switch (type) {
        case SOCK_RAW: {
            if (!to || tolen < 0 || (size_t)tolen < sizeof(sockaddr_in6))
                return nt_status::INVALID_PARAMETER;

            return sendto_raw(span(us_buf, len), flags, *(sockaddr_in6*)to,
                              sent_bytes);
        }

        default:
            return nt_status::INVALID_PARAMETER;
    }

    return nt_status::NOT_IMPLEMENTED;
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

static nt_status ipv6_choose_interface(const ipv6_address& dst, uint32_t scope_id,
                                       netif*& ni, ipv6_address& src) {
    // see RFC6724

    // FIXME - check if bind has been called

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
        return nt_status::NETWORK_UNREACHABLE;

    // FIXME - consult routing tables

    // FIXME - rule 3: avoid deprecated address
    // rule 4: prefer home addresses (Mobile IPv6, not relevant)
    // FIXME - rule 5: prefer outgoing interface
    // FIXME - rule 6: prefer matching label
    // FIXME - rule 7: prefer temporary addresses
    // FIXME - rule 8: prefer longest matching prefix

    print("FIXME: ipv6_choose_interface could not choose\n"); // FIXME - remove once rule 8 working

    ni->release();

    return nt_status::NETWORK_UNREACHABLE;
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

    pkt.ns.checksum = calc_icmpv6_checksum(pkt.ip, span((uint8_t*)&pkt.ns, sizeof(pkt.ns) + sizeof(pkt.opt)));

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

    {
        lock_guard lock(ni.neighbour_lock);

        if (nce->status != neighbour_cache_status::reachable) {
            remove_entry_list(nce->le);
            nce->release();
        }
    }

    nce->release();

    return NULL_MAC_ADDRESS;
}

static mac_address find_mac_address(netif& ni, const ipv6_address& dst) {
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

    if (is_link_local(dst)) // FIXME - or part of router prefix
        return search_neighbour_cache(ni, dst);
    else {
        // FIXME - off-link addresses (send to router's MAC)
    }

    return NULL_MAC_ADDRESS;
}

nt_status ipv6_socket::sendto_raw(span<const char> us_buf, int flags,
                                  const sockaddr_in6& to, size_t& sent_bytes) {
    nt_status status;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    auto& dst = *(ipv6_address*)to.sin6_addr;

    netif* pni;
    ipv6_address src;

    status = ipv6_choose_interface(dst, to.sin6_scope_id, pni, src);
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
    else {
        mac = find_mac_address(ni, dst);

        if (mac == NULL_MAC_ADDRESS) {
            ni.release();
            return nt_status::HOST_UNREACHABLE;
        }
    }

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
                             (uint32_t)us_buf.size(), nullptr);
    if (!success(status)) {
        pack->release();
        ni.release();
        return status;
    }

    // FIXME - should only be if IPV6_CHECKSUM sockopt set
    if (protocol == IPPROTO_ICMPV6 && us_buf.size() >= sizeof(icmpv6_header)) {
        auto& icmp = *(icmpv6_header*)(pack->data + sizeof(ipv6_header));

        icmp.checksum = 0;
        icmp.checksum = calc_icmpv6_checksum(ip, span((uint8_t*)&icmp, us_buf.size()));
    }

    if (src == dst)
        handle_ipv6_packet(ni, *pack, ni.mac);
    else {
        if (is_multicast_address(ip.dst) && should_accept_multicast(ni, ip.dst))
            handle_ipv6_packet(ni, *pack, ni.mac); // send copy to ourselves

        status = ni.send_packet(mac, ethertype::ipv6, *pack);
        if (!success(status)) {
            pack->release();
            ni.release();
            return status;
        }
    }

    pack->release();
    ni.release();

    sent_bytes = us_buf.size();

    return status;
}

netif::~netif() {
    while (true) {
        ipv6_prefix* pref = nullptr;

        {
            lock_guard lock(prefix_lock);

            if (!is_list_empty(prefixes)) {
                auto& p = containing_record<ipv6_prefix>(prefixes.next);

                remove_entry_list(p.le);
                pref = &p;
            }
        }

        if (!pref)
            break;

        delete pref;
    }
}
