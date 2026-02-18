module;

#include <array>
#include <span>
#include <mutex>
#include <bit>
#include <stdint.h>

module net;

import print;
import object_ref;
import handle;
import mem;
import moxnox_ioctls;
import thread;
import ws2_32;
import config;
import scheduler;

using namespace std;

static const uint8_t DEFAULT_HOP_LIMIT = 64;
static const size_t MAX_BUFFER_SIZE = 1048576;

static object_ref netctl_obj;

static spinlock ipv6_sockets_lock;
static list_entry ipv6_sockets = { &ipv6_sockets, &ipv6_sockets };
static size_t num_ipv6_sockets = 0;

struct ipv6_socket_packet {
    list_entry le;
    ipv6_address src;
    ipv6_address dst;
    size_t size;
    uint8_t data[0];
};

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
    pkt.ip.next_header = (uint8_t)ip_proto::icmpv6;
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

static void send_to_sockets(span<const uint8_t> buf) {
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

        auto p = (ipv6_socket_packet*)malloc(offsetof(ipv6_socket_packet, data) + buf.size());
        if (!p) {
            print("malloc failed\n");
            s.release();
            continue;
        }

        p->src = ip.src;
        p->dst = ip.dst;
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

void handle_ipv6_packet(network_interface& netif, span<const uint8_t> buf) {
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
            handle_icmpv6_packet(netif, buf);
            break;

        default:
            break;
    }

    send_to_sockets(span(buf.data() + sizeof(ethernet_header),
                         buf.size() - sizeof(ethernet_header)));
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

bool init_netctl() {
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

nt_status netctl::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t> /*outbuf*/, uintptr_t& /*written*/) {
    switch (code) {
        case IOCTL_MOXNOX_NETCTL_SOCKET:
            return netctl_socket(inbuf);

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

nt_status socket::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t>, uintptr_t&) {
    switch (code) {
        case IOCTL_MOXNOX_SOCKET_RECV: {
            if (inbuf.size() < sizeof(MOXNOX_SOCKET_RECV))
                return nt_status::BUFFER_TOO_SMALL;

            auto& t = *get_current_thread();
            if (!t.proc)
                return nt_status::INTERNAL_ERROR;

            auto& params = *(MOXNOX_SOCKET_RECV*)inbuf.data();
            size_t recv_bytes;

            if (!params.buf || params.len <= 0 || !params.recv_bytes)
                return nt_status::INVALID_PARAMETER;

            // FIXME - do directly into userspace memory

            // FIXME - jumbo frames, TCP, etc.
            if ((size_t)params.len > PAGE_SIZE)
                params.len = PAGE_SIZE;

            auto buf = (char*)malloc(params.len);
            if (!buf) {
                print("malloc failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            auto status = recv(buf, params.len, params.flags, recv_bytes);
            if (!success(status)) {
                free(buf);
                return status;
            }

            // FIXME - if MSG_TRUNC, recv_bytes might be more than len?
            status = t.proc->write_vm(params.buf, buf, (uint32_t)recv_bytes,
                                      nullptr);
            if (!success(status)) {
                free(buf);
                return status;
            }

            free(buf);

            status = t.proc->write_vm(params.recv_bytes, &recv_bytes,
                                      sizeof(size_t), nullptr);
            if (!success(status))
                return status;

            return nt_status::SUCCESS;
        }

        default:
            return nt_status::INVALID_PARAMETER;
    }
}

nt_status ipv6_socket::recv(char* buf, int len, int flags, size_t& recv_bytes) {
    // FIXME - MSG_TRUNC
    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    // FIXME - copy directly into userspace

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

        size_t size = min(sp->size, (size_t)len);

        memcpy(buf, sp->data, size);
        free(sp);

        recv_bytes = size;

        break;
    }

    release();

    return nt_status::SUCCESS;
}
