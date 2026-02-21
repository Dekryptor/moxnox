module;

#include <array>
#include <span>
#include <mutex>
#include <bit>
#include <atomic>
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

using namespace std;

static const uint8_t DEFAULT_HOP_LIMIT = 64;
static const size_t MAX_BUFFER_SIZE = 1048576;
static const mac_address NULL_MAC_ADDRESS = { 0, 0, 0, 0, 0, 0 };

static object_ref netctl_obj;

static spinlock ipv6_sockets_lock;
static list_entry ipv6_sockets = { &ipv6_sockets, &ipv6_sockets };
static size_t num_ipv6_sockets = 0;

struct ipv6_socket_packet {
    list_entry le;
    ipv6_address src;
    uint32_t scope_id;
    size_t size;
    uint8_t data[0];
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

static void send_neighbour_advertisement(netif& ni, const ipv6_address& target,
                                         const ipv6_address& dst_ip,
                                         const mac_address& dst_mac) {
    struct {
        ethernet_header eth;
        ipv6_header ip;
        icmpv6_neighbour_advertisement na;
        ndp_option_lladdr opt;
    } __attribute__((packed)) pkt;

    memset(&pkt, 0, sizeof(pkt));

    pkt.eth.src_mac = ni.mac;
    pkt.eth.dst_mac = dst_mac;

    pkt.eth.type = ethertype::ipv6;

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

    auto status = ni.send_packet(span((const uint8_t*)&pkt, sizeof(pkt)));
    if (!success(status))
        print("send_packet returned {}\n", status);
}

static void handle_neighbour_solicitation(netif& ni, span<const uint8_t> buf) {
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

    send_neighbour_advertisement(ni, ns.target, ip.src, eth.src_mac);
}

static void handle_neighbour_advertisement(netif& ni, span<const uint8_t> buf) {
    if (buf.size() < sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement)) {
        print("handle_neighbour_advertisement: buffer length was {}, expected at least {}\n",
              buf.size(), sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement));
        return;
    }

    auto& na = *(icmpv6_neighbour_advertisement*)(buf.data() + sizeof(ethernet_header) + sizeof(ipv6_header));

#if 0
    print("na: override = {}, solicited = {}, router = {}, target = {}\n",
          na._override, na.solicited, na.router, na.target);
#endif

    auto opts = span(buf.data() + sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_neighbour_advertisement),
                     buf.size() - sizeof(ethernet_header) - sizeof(ipv6_header) - sizeof(icmpv6_neighbour_advertisement));

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

    {
        lock_guard lock(ni.neighbour_lock);

        for (auto le = ni.neighbour_cache.next; le != &ni.neighbour_cache; le = le->next) {
            auto& c = containing_record<neighbour_cache_entry>(le);

            if (c.addr != na.target)
                continue;

            if (!na._override && c.status == neighbour_cache_status::reachable)
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

    nce->addr = na.target;

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

            if (c.addr == na.target) {
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

static void handle_v6_echo_request(netif& ni, span<const uint8_t> buf) {
    const auto& orig_ip = *(ipv6_header*)(buf.data() + sizeof(ethernet_header));

    // check if it's for us

    bool found = false;
    {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
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

    auto status = ni.send_packet(span(reply, buf.size()));
    if (!success(status))
        print("send_packet returned {}\n", status);

    free(reply);
}

static void handle_icmpv6_packet(netif& ni, span<const uint8_t> buf) {
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
            handle_neighbour_solicitation(ni, buf);
            break;

        case icmpv6_type::neighbour_advertisement:
            handle_neighbour_advertisement(ni, buf);
            break;

        case icmpv6_type::echo_request:
            handle_v6_echo_request(ni, buf);
            break;

        default:
            break;
    }
}

static void send_to_sockets(uint32_t netif_num, span<const uint8_t> buf) {
    auto& ip = *(ipv6_header*)buf.data();

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

        // FIXME - strip headers if necessary

        auto p = (ipv6_socket_packet*)malloc(offsetof(ipv6_socket_packet, data) + buf.size() - sizeof(ipv6_header));
        if (!p) {
            print("malloc failed\n");
            s.release();
            continue;
        }

        p->src = ip.src;
        p->scope_id = netif_num;
        p->size = buf.size() - sizeof(ipv6_header);
        memcpy(p->data, buf.data() + sizeof(ipv6_header),
               buf.size() - sizeof(ipv6_header));

        bool done = false;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(s.buffer_lock);

            if (s.buffer_size + buf.size() <= MAX_BUFFER_SIZE) {
                insert_tail_list(s.buffer, p->le);
                s.buffer_size += buf.size();
                done = true;
            }

            if (!s.signalled) {
                wake_up_waiters(s.waiters);
                s.signalled = true;
            }
        }

        if (!done) {
            free(p);
            s.release();
            continue;
        }

        s.release();
    }

    free(arr);
}

void handle_ipv6_packet(netif& ni, span<const uint8_t> buf) {
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

    switch ((ip_proto)header.next_header) {
        case ip_proto::icmpv6:
            handle_icmpv6_packet(ni, buf);
            break;

        default:
            break;
    }

    send_to_sockets(ni.num, span(buf.data() + sizeof(ethernet_header),
                                 buf.size() - sizeof(ethernet_header)));

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

static void network_housekeeping() {
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

        free(pp);
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

        auto to_copy = (uint32_t)min(sp->size, len);

        status = t.proc->write_vm(us_buf, sp->data, to_copy, nullptr);
        if (!success(status)) {
            free(sp);
            release();
            return status;
        }

        recv_bytes = to_copy;

        if (us_from && fromlen && *fromlen > 0) {
            addr.sin6_family = AF_INET6;
            addr.sin6_port = 0;
            addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does
            memcpy(addr.sin6_addr, sp->src.v.data(), sizeof(ipv6_address));
            addr.sin6_scope_id = sp->scope_id;

            status = t.proc->write_vm(us_from, &addr,
                                      (uint32_t)min((size_t)*fromlen, sizeof(addr)),
                                      nullptr);
            if (!success(status)) {
                free(sp);
                release();
                return status;
            }

            *fromlen = sizeof(addr);
        }

        free(sp);

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

    // FIXME - consult routing tables

    // FIXME - rule 3: avoid deprecated address
    // rule 4: prefer home addresses (Mobile IPv6, not relevant)
    // FIXME - rule 5: prefer outgoing interface
    // FIXME - rule 6: prefer matching label
    // FIXME - rule 7: prefer temporary addresses
    // FIXME - rule 8: prefer longest matching prefix

    print("FIXME: ipv6_choose_interface could not choose\n"); // FIXME - remove once rule 8 working

    return nt_status::NETWORK_UNREACHABLE;
}

static void send_neighbour_solicitation(netif& ni, const ipv6_address& dst) {
    struct {
        ethernet_header eth;
        ipv6_header ip;
        icmpv6_neighbour_solicitation ns;
        ndp_option_lladdr opt;
    } __attribute__((packed)) pkt;

    memset(&pkt, 0, sizeof(pkt));

    pkt.eth.src_mac = ni.mac;

    pkt.eth.dst_mac.v[0] = 0x33;
    pkt.eth.dst_mac.v[1] = 0x33;
    pkt.eth.dst_mac.v[2] = 0xff;
    pkt.eth.dst_mac.v[3] = dst.v[13];
    pkt.eth.dst_mac.v[4] = dst.v[14];
    pkt.eth.dst_mac.v[5] = dst.v[15];

    pkt.eth.type = ethertype::ipv6;

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

    auto status = ni.send_packet(span((const uint8_t*)&pkt, sizeof(pkt)));
    if (!success(status))
        print("send_packet returned {}\n", status);
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

    auto mac = find_mac_address(ni, dst);

    if (mac == NULL_MAC_ADDRESS) {
        ni.release();
        return nt_status::HOST_UNREACHABLE;
    }

    size_t pktsize = sizeof(ethernet_header) + sizeof(ipv6_header) + us_buf.size();
    auto pkt = (uint8_t*)malloc(pktsize);
    if (!pkt) {
        print("malloc failed\n");
        ni.release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto& eth = *(ethernet_header*)pkt;
    eth.dst_mac = mac;
    eth.src_mac = ni.mac;
    eth.type = ethertype::ipv6;

    auto& ip = *(ipv6_header*)(pkt + sizeof(ethernet_header));
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label = 0;
    ip.payload_length = byteswap((uint16_t)us_buf.size());
    ip.next_header = (uint8_t)protocol;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = src;
    ip.dst = dst;

    status = t.proc->read_vm(us_buf.data(),
                             pkt + sizeof(ethernet_header) + sizeof(ipv6_header),
                             (uint32_t)us_buf.size(), nullptr);
    if (!success(status)) {
        free(pkt);
        ni.release();
        return status;
    }

    // FIXME - should only be if IPV6_CHECKSUM sockopt set
    if (protocol == IPPROTO_ICMPV6 && pktsize >= sizeof(ethernet_header) + sizeof(ipv6_header) + sizeof(icmpv6_header)) {
        auto& icmp = *(icmpv6_header*)(pkt + sizeof(ethernet_header) + sizeof(ipv6_header));

        icmp.checksum = 0;
        icmp.checksum = calc_icmpv6_checksum(ip, span((uint8_t*)&icmp, pktsize - sizeof(ethernet_header) - sizeof(ipv6_header)));
    }

    status = ni.send_packet(span(pkt, pktsize));
    if (!success(status)) {
        free(pkt);
        ni.release();
        return status;
    }

    ni.release();

    sent_bytes = us_buf.size();

    return status;
}
