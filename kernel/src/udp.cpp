module;

#include <utility>
#include <mutex>
#include <algorithm>
#include <span>
#include <bit>
#include <atomic>

module net;

import print;
import object_ref;
import thread;
import scheduler;
import kernel_time_page;
import config;
import vec;

using namespace std;

template<socket_family FAMILY>
class udp_socket : public socket {
public:
    ~udp_socket() override;
    void last_handle_closed() override;

    nt_status recvfrom(span<const span<uint8_t>> v, int flags,
                       sockaddr_storage* from, size_t* fromlen,
                       uint64_t& recv_bytes, span<HANDLE>, size_t&) override {
        return do_recvfrom<false>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto(span<const span<const uint8_t>> v, int flags, sockaddr* to,
                     size_t tolen, uint64_t& sent_bytes,
                     span<const HANDLE> handles) override {
        if (!handles.empty())
            return nt_status::NOT_SUPPORTED;

        return do_sendto<false>(v, flags, to, tolen, sent_bytes);
    }

    nt_status bind(const sockaddr* addr, size_t addrlen) override;
    nt_status connect(const sockaddr* addr, size_t addrlen) override;

    nt_status listen(int) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status accept(HANDLE&, void*, int&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status getsockname(span<uint8_t> buf, size_t& len) override;
    nt_status getpeername(span<uint8_t> buf, size_t& len) override;

    nt_status recvfrom_nonblock(span<const span<uint8_t>> v, int flags,
                                sockaddr_storage* from, size_t* fromlen,
                                uint64_t& recv_bytes, span<HANDLE>, size_t&) override {
        return do_recvfrom<true>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto_nonblock(span<const span<const uint8_t>> v, int flags,
                              sockaddr* to, size_t tolen,
                              uint64_t& sent_bytes, span<const HANDLE> handles) override {
        if (!handles.empty())
            return nt_status::NOT_SUPPORTED;

        return do_sendto<true>(v, flags, to, tolen, sent_bytes);
    }

    nt_status shutdown(int how) override;

    nt_status setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) override;

    spinlock buffer_lock;
    list_entry buffer = { &buffer, &buffer };
    size_t buffer_size = 0;
    spinlock bind_lock;
    bool bind_done = false, connect_done = false, autobound = false;
    uint16_t bound_port, connected_port;
    uint32_t connected_scope = 0;
    typename FAMILY::address bound_address, connected_address;
    nt_status pending_error = nt_status::SUCCESS;
    atomic<bool> dead = false, shutdown_read = false, shutdown_write = false;

private:
    template<bool NONBLOCK>
    nt_status do_recvfrom(span<const span<uint8_t>> v, int flags,
                          sockaddr_storage* from, size_t* fromlen,
                          uint64_t& recv_bytes);

    template<bool NONBLOCK>
    nt_status do_sendto(span<const span<const uint8_t>> v, int flags,
                        sockaddr* to, size_t tolen, uint64_t& sent_bytes);
};

static spinlock udp_ports_lock;
static udp_socket<ipv4>* udp_ports_v4[65536];
static udp_socket<ipv6>* udp_ports_v6[65536];

struct udp_header {
    uint16_t srcport;
    uint16_t dstport;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

static_assert(sizeof(udp_header) == 8);

static const size_t MAX_BUFFER_SIZE = 1048576;

static void send_port_unreachable_v4(netif& ni, const packet& pack,
                                     const mac_address& mac) {
    static const size_t MIN_MTU = 576;

    const auto& orig_ip = *(ipv4_header*)pack.data;

    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    size_t payload_size = min((size_t)byteswap((uint16_t)orig_ip.total_length),
                              MIN_MTU - sizeof(ipv4_header) - sizeof(icmpv4_error));

    pack2->size = sizeof(ipv4_header) + offsetof(icmpv4_error, header) + payload_size;

    auto& ip = *(ipv4_header*)pack2->data;
    ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
    ip.version = 4;
    ip.ecn = 0;
    ip.dscp = 0;
    ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + offsetof(icmpv4_error, header) + payload_size));
    ip.identification = 0;
    ip.fragment_offset1 = 0;
    ip.MF = 0;
    ip.DF = 1;
    ip.R = 0;
    ip.fragment_offset2 = 0;
    ip.ttl = DEFAULT_HOP_LIMIT;
    ip.protocol = IPPROTO_ICMP;
    ip.header_checksum = 0;
    ip.src = orig_ip.dst; // FIXME - make sure not multicast?
    ip.dst = orig_ip.src;

    ip.header_checksum = calc_ipv4_checksum(span(pack2->data, sizeof(ipv4_header)));

    auto& err = *(icmpv4_error*)(pack2->data + sizeof(ipv4_header));

    err.type = icmpv4_type::dest_unreachable;
    err.code = (uint8_t)icmpv4_dest_unreachable_code::dest_port_unreachable;
    err.checksum = 0;
    err.param = 0;

    memcpy(&err.header, pack.data, payload_size);

    err.checksum = calc_ipv4_checksum(span((uint8_t*)&err, offsetof(icmpv4_error, header) + payload_size));

    auto status = ni.send_packet(mac, ethertype::ipv4, *pack2);
    if (!success(status))
        print("netif::send_packet returned {}\n", status);

    pack2->release();
}

static void send_port_unreachable_v6(netif& ni, const packet& pack,
                                     const mac_address& mac) {
    static const size_t MIN_MTU = 1280;

    const auto& orig_ip = *(ipv6_header*)pack.data;

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
    ip.flow_label_high = 0;
    ip.flow_label_medium = 0;
    ip.flow_label_low = 0;
    ip.payload_length = byteswap((uint16_t)(offsetof(icmpv6_error, header) + payload_size));
    ip.next_header = IPPROTO_ICMPV6;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = orig_ip.dst; // FIXME - make sure not multicast?
    ip.dst = orig_ip.src;

    auto& err = *(icmpv6_error*)(pack2->data + sizeof(ipv6_header));

    err.type = icmpv6_type::dest_unreachable;
    err.code = (uint8_t)icmpv6_dest_unreachable_code::no_port;
    err.checksum = 0;
    err.reserved = 0;

    memcpy(&err.header, pack.data, payload_size);

    err.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&err, offsetof(icmpv6_error, header) + payload_size), IPPROTO_ICMPV6);

    auto status = ni.send_packet(mac, ethertype::ipv6, *pack2);
    if (!success(status))
        print("netif::send_packet returned {}\n", status);

    pack2->release();
}

void received_ipv4_udp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum) {
    if (pack.size < sizeof(ipv4_header))
        return;

    auto& ip = *(ipv4_header*)pack.data;
    auto ip_header_size = ip.ihl * sizeof(uint32_t);

    if (ip_header_size < sizeof(ipv4_header))
        return;

    if (pack.size < ip_header_size + sizeof(udp_header))
        return;

    if (is_multicast_address(ip.dst))
        return;

    auto& udp = *(udp_header*)(pack.data + ip_header_size);

    if (byteswap(udp.length) < sizeof(udp_header))
        return;

    if (pack.size < ip_header_size + byteswap(udp.length))
        return;

    if (!skip_checksum && udp.checksum != 0) {
        auto checksum = calc_ipv4_checksum(ip, span((uint8_t*)&udp, min((size_t)byteswap(udp.length), pack.size - ip_header_size)), IPPROTO_UDP);

        if (checksum != 0 && checksum != 0xffff)
            return;
    }

#if 0
    print("received_udp_packet (src {}, dst {}, {}, {:04x})\n",
          byteswap(udp.srcport), byteswap(udp.dstport), byteswap(udp.length),
          byteswap(udp.checksum));
#endif

    udp_socket<ipv4>* sock = nullptr;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v4[udp.dstport]) {
            sock = udp_ports_v4[udp.dstport];

            {
                lock_guard lock2(sock->bind_lock);

                if (sock->bind_done && sock->bound_address != parse_ipv4_address("0.0.0.0") && sock->bound_address != ip.dst)
                    sock = nullptr;
                else
                    sock->acquire();
            }
        }
    }

    if (!sock) {
        if (!is_broadcast_address(ni, ip.dst))
            send_port_unreachable_v4(ni, pack, src_mac);

        return;
    }

    {
        bool ignore = false;

        {
            lock_guard lock(sock->bind_lock);

            if (sock->connect_done && (sock->connected_address != ip.src || sock->connected_port != udp.srcport))
                ignore = true;
        }

        if (ignore) {
            sock->release();
            return;
        }
    }

    auto p = new socket_packet(pack);
    if (!p) {
        print("new failed\n");
        sock->release();
        return;
    }

    p->scope_id = ni.num;

    bool done = false;

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (sock->buffer_size + pack.size <= MAX_BUFFER_SIZE) {
            insert_tail_list(sock->buffer, p->le);
            sock->buffer_size += pack.size;
            done = true;
        }

        if (!sock->signalled) {
            wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
            sock->signalled = true;
        }
    }

    if (!done)
        delete p;

    sock->release();
}

void received_ipv6_udp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac, bool skip_checksum) {
    if (pack.size < sizeof(ipv6_header) + sizeof(udp_header))
        return;

    auto& ip = *(ipv6_header*)pack.data;

    if (is_multicast_address(ip.dst))
        return;

    auto& udp = *(udp_header*)(pack.data + sizeof(ipv6_header));

    if (!skip_checksum) {
        if (calc_ipv6_checksum(ip, span((uint8_t*)&udp, pack.size - sizeof(ipv6_header)), IPPROTO_UDP) != 0)
            return;
    }

#if 0
    print("received_udp_packet (src {}, dst {}, {}, {:04x})\n",
          byteswap(udp.srcport), byteswap(udp.dstport), byteswap(udp.length),
          byteswap(udp.checksum));
#endif

    udp_socket<ipv6>* sock = nullptr;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v6[udp.dstport]) {
            sock = udp_ports_v6[udp.dstport];

            {
                lock_guard lock2(sock->bind_lock);

                if (sock->bind_done && sock->bound_address != parse_ipv6_address("::") && sock->bound_address != ip.dst)
                    sock = nullptr;
                else
                    sock->acquire();
            }
        }
    }

    if (!sock) {
        send_port_unreachable_v6(ni, pack, src_mac);
        return;
    }

    {
        bool ignore = false;

        {
            lock_guard lock(sock->bind_lock);

            if (sock->connect_done && (sock->connected_address != ip.src || sock->connected_port != udp.srcport))
                ignore = true;
        }

        if (ignore) {
            sock->release();
            return;
        }
    }

    auto p = new socket_packet(pack);
    if (!p) {
        print("new failed\n");
        sock->release();
        return;
    }

    p->scope_id = ni.num;

    bool done = false;

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (sock->buffer_size + pack.size <= MAX_BUFFER_SIZE) {
            insert_tail_list(sock->buffer, p->le);
            sock->buffer_size += pack.size;
            done = true;
        }

        if (!sock->signalled) {
            wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
            sock->signalled = true;
        }
    }

    if (!done)
        delete p;

    sock->release();
}

template<socket_family FAMILY>
udp_socket<FAMILY>::~udp_socket() {
    while (true) {
        socket_packet* pp = nullptr;

        {
            lock_guard lg(buffer_lock);

            if (is_list_empty(buffer))
                break;

            auto& p = containing_record<socket_packet>(buffer.next);

            remove_entry_list(p.le);
            pp = &p;
        }

        if (!pp)
            break;

        delete pp;
    }
}

template<socket_family FAMILY>
template<bool NONBLOCK>
nt_status udp_socket<FAMILY>::do_recvfrom(span<const span<uint8_t>> v, int flags,
                                          sockaddr_storage* from, size_t* fromlen,
                                          uint64_t& recv_bytes) {
    // FIXME - MSG_TRUNC
    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    recv_bytes = 0;

    auto vec_size = get_vec_size(v);
    span<uint8_t> top = vec_size == 0 ? span<uint8_t>() : v.front();

    acquire();

    while (true) {
        nt_status status;

        if constexpr (NONBLOCK) {
            status = wait_for_object(*this, true, 0);

            if (status == nt_status::TIMEOUT) {
                release();
                return nt_status::CANT_WAIT;
            } else if (!success(status) || status == nt_status::USER_APC) {
                release();
                return status;
            }
        } else {
            status = wait_for_object(*this, true);
            if (!success(status) || status == nt_status::USER_APC) {
                release();
                return status;
            }
        }

        if (dead) {
            release();
            return nt_status::CONNECTION_DISCONNECTED;
        } else if (shutdown_read)
            break;

        socket_packet* sp = nullptr;
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
                sp = &containing_record<socket_packet>(buffer.next);
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

        size_t overhead;
        uint32_t to_copy;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            auto& ip = *(ipv4_header*)sp->pack.data;
            auto& udp = *(udp_header*)(sp->pack.data + (ip.ihl * sizeof(uint32_t)));

            overhead = (ip.ihl * sizeof(uint32_t)) + sizeof(udp_header);
            to_copy = (uint32_t)min(byteswap(udp.length) - sizeof(udp_header), vec_size);
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            overhead = sizeof(typename FAMILY::header) + sizeof(udp_header);
            to_copy = (uint32_t)min(sp->pack.size - overhead, vec_size);
        }

        memcpy_vec(v, top, sp->pack.data + overhead, to_copy);

        recv_bytes = to_copy;

        if (from && fromlen && *fromlen > 0) {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                auto& addr = *(sockaddr_in*)from;
                auto& ip = *(ipv4_header*)sp->pack.data;
                auto& udp = *(udp_header*)(sp->pack.data + (ip.ihl * sizeof(uint32_t)));

                memset(&addr, 0, sizeof(addr));

                addr.sin_family = AF_INET;
                addr.sin_port = udp.srcport;
                memcpy(&addr.sin_addr, ip.src.v.data(), sizeof(ipv4_address));

                *fromlen = sizeof(addr);
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                auto& addr = *(sockaddr_in6*)from;
                auto& ip = *(ipv6_header*)sp->pack.data;
                auto& udp = *(udp_header*)(sp->pack.data + sizeof(ipv6_header));

                memset(&addr, 0, sizeof(addr));

                addr.sin6_family = AF_INET6;
                addr.sin6_port = udp.srcport;
                addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does
                memcpy(&addr.sin6_addr, ip.src.v.data(), sizeof(ipv6_address));
                addr.sin6_scope_id = sp->scope_id;

                *fromlen = sizeof(addr);
            }
        }

        delete sp;

        break;
    }

    release();

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
uint16_t find_ephemeral_udp_port() {
    auto rnd = kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE);
    uint16_t port = 49152 + (rnd & 0x3fff);
    uint16_t orig_port = port;

    while (true) {
        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (!udp_ports_v4[byteswap(port)])
                return byteswap(port);
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            if (!udp_ports_v6[byteswap(port)])
                return byteswap(port);
        }

        if (port == 65535)
            port = 49152;
        else
            port++;

        if (port == orig_port)
            return 0;
    }
}

template<socket_family FAMILY>
template<bool NONBLOCK>
nt_status udp_socket<FAMILY>::do_sendto(span<const span<const uint8_t>> v,
                                        int flags, sockaddr* to, size_t tolen,
                                        uint64_t& sent_bytes) {
    nt_status status;
    typename FAMILY::address src;
    uint16_t srcport;
    bool connected;
    typename FAMILY::sockaddr dest;
    auto vec_size = get_vec_size(v);
    span<const uint8_t> top = vec_size == 0 ? span<const uint8_t>() : v.front();

    {
        lock_guard lock(bind_lock);
        connected = this->connect_done;

        if (connected) {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                *(ipv4_address*)&dest.sin_addr = connected_address;
                dest.sin_port = connected_port;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                *(ipv6_address*)&dest.sin6_addr = connected_address;
                dest.sin6_port = connected_port;
                dest.sin6_scope_id = connected_scope;
            }
        }
    }

    if (!connected && (!to || tolen < sizeof(typename FAMILY::sockaddr)))
        return nt_status::INVALID_PARAMETER;

    // if connected, sendto destination is ignored
    auto& to6 = connected ? dest : *(typename FAMILY::sockaddr*)to;

    if (shutdown_write)
        return nt_status::CONNECTION_ABORTED;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    bool has_error = false;

    {
        lock_guard lock(buffer_lock);
        has_error = !success(pending_error);
    }

    if (has_error) { // avoid getting sync_lock unless we have to
        nt_status err = nt_status::SUCCESS;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(buffer_lock);

            if (!success(pending_error)) {
                err = pending_error;
                pending_error = nt_status::SUCCESS;

                if (is_list_empty(buffer))
                    signalled = false;
            }
        }

        if (!success(err))
            return err;
    }

    // get source port (implicit bind on first sendto)

    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (!bind_done) {
            srcport = find_ephemeral_udp_port<FAMILY>();
            if (srcport == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;

            if constexpr (is_same_v<FAMILY, ipv4>)
                udp_ports_v4[srcport] = this;
            else if constexpr (is_same_v<FAMILY, ipv6>)
                udp_ports_v6[srcport] = this;

            this->acquire();

            bound_port = srcport;

            if constexpr (is_same_v<FAMILY, ipv4>)
                bound_address = parse_ipv4_address("0.0.0.0");
            else if constexpr (is_same_v<FAMILY, ipv6>)
                bound_address = parse_ipv6_address("::");

            bind_done = true;
            autobound = true;
        }

        srcport = bound_port;
        src = bound_address;
    }

    typename FAMILY::address dst, gateway;

    if constexpr (is_same_v<FAMILY, ipv4>)
        dst = *(typename FAMILY::address*)&to6.sin_addr;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        dst = *(typename FAMILY::address*)&to6.sin6_addr;

    netif* pni;

    if constexpr (is_same_v<FAMILY, ipv4>)
        status = ipv4_choose_interface(dst, pni, src, gateway);
    else if constexpr (is_same_v<FAMILY, ipv6>)
        status = ipv6_choose_interface(dst, to6.sin6_scope_id, pni, src);

    if (!success(status))
        return status;

    auto& ni = *pni;

    if (vec_size + sizeof(typename FAMILY::header) + sizeof(udp_header) > ni.mtu) {
        ni.release();
        return nt_status::INVALID_BUFFER_SIZE;
    }

    mac_address mac;

    if (src == dst)
        mac = ni.mac;
    else {
        if constexpr (is_same_v<FAMILY, ipv4>)
            mac = find_mac_address(ni, gateway == parse_ipv4_address("0.0.0.0") ? dst : gateway);
        else if constexpr (is_same_v<FAMILY, ipv6>)
            mac = find_mac_address(ni, dst);
    }

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    pack->size = sizeof(typename FAMILY::header) + sizeof(udp_header) + vec_size;

    auto& udp = *(udp_header*)(pack->data + sizeof(typename FAMILY::header));
    udp.srcport = srcport;

    if constexpr (is_same_v<FAMILY, ipv4>)
        udp.dstport = to6.sin_port;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        udp.dstport = to6.sin6_port;

    udp.length = byteswap((uint16_t)(sizeof(udp_header) + vec_size));
    udp.checksum = 0;

    memcpy_vec(pack->data + sizeof(typename FAMILY::header) + sizeof(udp_header),
               v, top, vec_size);

    if constexpr (is_same_v<FAMILY, ipv4>) {
        auto& ip = *(ipv4_header*)pack->data;

        ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
        ip.version = 4;
        ip.ecn = 0;
        ip.dscp = 0;
        ip.total_length = byteswap((uint16_t)(sizeof(ipv4_header) + sizeof(udp_header) + vec_size));
        ip.identification = 0;
        ip.fragment_offset1 = 0;
        ip.MF = 0;
        ip.DF = 1;
        ip.R = 0;
        ip.fragment_offset2 = 0;
        ip.ttl = DEFAULT_HOP_LIMIT;
        ip.protocol = IPPROTO_UDP;
        ip.header_checksum = 0;
        ip.src = src;
        ip.dst = dst;

        ip.header_checksum = calc_ipv4_checksum(span(pack->data, sizeof(ipv4_header)));

        udp.checksum = calc_ipv4_checksum(ip, span((uint8_t*)&udp, pack->size - sizeof(ipv4_header)),
                                          IPPROTO_UDP);

        if (udp.checksum == 0)
            udp.checksum = 0xffff;
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        auto& ip = *(ipv6_header*)pack->data;
        ip.traffic_class1 = 0;
        ip.version = 6;
        ip.traffic_class2 = 0;
        ip.flow_label_high = 0;
        ip.flow_label_medium = 0;
        ip.flow_label_low = 0;
        ip.payload_length = byteswap((uint16_t)(sizeof(udp_header) + vec_size));
        ip.next_header = IPPROTO_UDP;
        ip.hop_limit = DEFAULT_HOP_LIMIT;
        ip.src = src;
        ip.dst = dst;

        if (!ni.tx_udpv6_csum_offload)
            udp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&udp, pack->size - sizeof(ipv6_header)),
                                              IPPROTO_UDP);
        else {
            udp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                      IPPROTO_UDP);
        }
    }

    if (mac == NULL_MAC_ADDRESS) {
        send_address_unreachable(ni, *pack, src);

        pack->release();
        ni.release();
        return nt_status::HOST_UNREACHABLE;
    }

    status = dispatch_packet<FAMILY>(ni, *pack, mac);
    if (!success(status)) {
        pack->release();
        ni.release();
        return status;
    }

    pack->release();
    ni.release();

    sent_bytes = vec_size;

    return status;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::bind(const sockaddr* addr, size_t addrlen) {
    if (addr->sa_family != FAMILY::family)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& b = *(typename FAMILY::sockaddr*)addr;
    typename FAMILY::address ip;

    if constexpr (is_same_v<FAMILY, ipv4>) {
        ip = *(typename FAMILY::address*)&b.sin_addr;

        if (ip != parse_ipv4_address("0.0.0.0")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    } else if constexpr (is_same_v<FAMILY, ipv6>) {
        ip = *(typename FAMILY::address*)&b.sin6_addr;

        if (ip != parse_ipv6_address("::")) {
            if (!is_our_ip(ip))
                return nt_status::INVALID_ADDRESS_COMPONENT;
        }
    }

    {
        uint16_t port;

        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (bind_done)
            return nt_status::INVALID_PARAMETER;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (b.sin_port == 0) {
                port = find_ephemeral_udp_port<FAMILY>();
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            } else {
                port = b.sin_port;

                if (udp_ports_v4[b.sin_port])
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            }

            udp_ports_v4[b.sin_port] = this;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            if (b.sin6_port == 0) {
                port = find_ephemeral_udp_port<FAMILY>();
                if (port == 0) // no ephemeral ports left
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            } else {
                port = b.sin6_port;

                if (udp_ports_v6[b.sin6_port])
                    return nt_status::ADDRESS_ALREADY_ASSOCIATED;
            }

            udp_ports_v6[b.sin6_port] = this;
        }

        this->acquire();

        bound_port = port;
        bound_address = ip;

        bind_done = true;
    }

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::connect(const sockaddr* addr, size_t addrlen) {
    nt_status status;

    if (addr->sa_family == AF_UNSPEC) {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        connect_done = false;

        if (bind_done && autobound) {
            if constexpr (is_same_v<FAMILY, ipv4>)
                udp_ports_v4[bound_port] = nullptr;
            else if constexpr (is_same_v<FAMILY, ipv6>)
                udp_ports_v6[bound_port] = nullptr;

            this->release();

            bind_done = false;
            autobound = false;
        }

        return nt_status::SUCCESS;
    }

    if (addr->sa_family != FAMILY::family)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& c = *(typename FAMILY::sockaddr*)addr;

    uint16_t port;
    typename FAMILY::address src;

    {
        netif* pni = nullptr;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            ipv4_address gateway;

            src = parse_ipv4_address("0.0.0.0");

            {
                lock_guard lock(bind_lock);

                if (bind_done)
                    src = bound_address;
            }

            status = ipv4_choose_interface(*(ipv4_address*)&c.sin_addr, pni,
                                        src, gateway);
            if (!success(status))
                return status;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            src = parse_ipv6_address("::");

            {
                lock_guard lock(bind_lock);

                if (bind_done)
                    src = bound_address;
            }

            status = ipv6_choose_interface(*(ipv6_address*)&c.sin6_addr,
                                           c.sin6_scope_id, pni, src);
            if (!success(status))
                return status;
        }

        if (pni)
            pni->release();
    }

    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (!bind_done) { // implicit bind if not already done
            port = find_ephemeral_udp_port<FAMILY>();
            if (port == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;

            if constexpr (is_same_v<FAMILY, ipv4>)
                udp_ports_v4[port] = this;
            else if constexpr (is_same_v<FAMILY, ipv6>)
                udp_ports_v6[port] = this;

            this->acquire();

            bound_port = port;
            bound_address = src;

            bind_done = true;
            autobound = true;
        }

        if constexpr (is_same_v<FAMILY, ipv4>) {
            connected_port = c.sin_port;
            connected_address = *(ipv4_address*)&c.sin_addr;
        } else {
            connected_port = c.sin6_port;
            connected_address = *(ipv6_address*)&c.sin6_addr;
            connected_scope = c.sin6_scope_id;
        }

        connect_done = true;
    }

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
void udp_socket<FAMILY>::last_handle_closed() {
    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if constexpr (is_same_v<FAMILY, ipv4>) {
            if (bind_done && udp_ports_v4[bound_port] == this) {
                udp_ports_v4[bound_port] = nullptr;
                this->release();
            }
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            if (bind_done && udp_ports_v6[bound_port] == this) {
                udp_ports_v6[bound_port] = nullptr;
                this->release();
            }
        }
    }

    dead = true;

    {
        sync_lock sl;
        lock_guard lock(sl);

        if (!signalled) {
            wake_up_waiters(waiters, WAIT_TYPE_READING);
            signalled = true;
        }
    }
}

void handle_icmpv4_error_udp(packet& pack) {
    const auto& orig_ip = *(ipv4_header*)pack.data;
    size_t ip_header_size = orig_ip.ihl * sizeof(uint32_t);
    const auto& icmp = *(icmpv4_error*)(pack.data + ip_header_size);

    if (pack.size < ip_header_size + sizeof(icmpv4_error) + sizeof(udp_header))
        return;

    const auto& udp = *(udp_header*)(pack.data + ip_header_size + sizeof(icmpv4_error));

    udp_socket<ipv4>* sock;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v4[udp.srcport]) {
            sock = udp_ports_v4[udp.srcport];
            sock->acquire();
        } else
            return;
    }

    bool drop = false;

    {
        lock_guard lock(sock->bind_lock);

        if (sock->bound_address != parse_ipv4_address("0.0.0.0") && sock->bound_address != icmp.header.src)
            drop = true;
        else if (sock->connect_done && (sock->connected_address != icmp.header.dst || sock->connected_port != udp.dstport))
            drop = true;
    }

    if (drop) {
        sock->release();
        return;
    }

    nt_status err;

    switch (icmp.type) {
        case icmpv4_type::dest_unreachable:
            switch ((icmpv4_dest_unreachable_code)icmp.code) {
                using enum icmpv4_dest_unreachable_code;

                case dest_net_unreachable:
                case dest_net_unknown:
                    err = nt_status::NETWORK_UNREACHABLE;
                    break;

                case host_administratively_prohibited:
                case net_administratively_prohibited:
                case communication_administratively_prohibited:
                    err = nt_status::ACCESS_DENIED;
                    break;

                case dest_host_unreachable:
                case dest_host_unknown:
                    err = nt_status::HOST_UNREACHABLE;
                    break;

                case dest_proto_unreachable:
                case dest_port_unreachable:
                    err = nt_status::CONNECTION_REFUSED;
                    break;

                default:
                    err = nt_status::HOST_UNREACHABLE;
                    break;
            }
            break;

        case icmpv4_type::time_exceeded:
            err = nt_status::HOST_UNREACHABLE;
            break;

        case icmpv4_type::param_problem:
            err = nt_status::PROTOCOL_UNREACHABLE;
            break;

        default:
            unreachable();
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (success(sock->pending_error)) {
            sock->pending_error = err;

            if (!sock->signalled) {
                wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
                sock->signalled = true;
            }
        }
    }

    sock->release();
}

void handle_icmpv6_error_udp(packet& pack) {
    const auto& icmp = *(icmpv6_error*)(pack.data + sizeof(ipv6_header));

    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_error) + sizeof(udp_header))
        return;

    const auto& udp = *(udp_header*)(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_error));

    udp_socket<ipv6>* sock;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports_v6[udp.srcport]) {
            sock = udp_ports_v6[udp.srcport];
            sock->acquire();
        } else
            return;
    }

    bool drop = false;

    {
        lock_guard lock(sock->bind_lock);

        if (sock->bound_address != parse_ipv6_address("::") && sock->bound_address != icmp.header.src)
            drop = true;
        else if (sock->connect_done && (sock->connected_address != icmp.header.dst || sock->connected_port != udp.dstport))
            drop = true;
    }

    if (drop) {
        sock->release();
        return;
    }

    nt_status err;

    switch (icmp.type) {
        case icmpv6_type::dest_unreachable:
            switch ((icmpv6_dest_unreachable_code)icmp.code) {
                case no_route:
                case reject_route:
                    err = nt_status::NETWORK_UNREACHABLE;
                    break;

                case administratively_prohibited:
                case policy_fail:
                    err = nt_status::ACCESS_DENIED;
                    break;

                case beyond_scope:
                case address_unreachable:
                    err = nt_status::HOST_UNREACHABLE;
                    break;

                case no_port:
                    err = nt_status::CONNECTION_REFUSED;
                    break;

                default:
                    err = nt_status::HOST_UNREACHABLE;
                    break;
            }
            break;

        case icmpv6_type::packet_too_big:
            err = nt_status::BUFFER_TOO_SMALL;
            break;

        case icmpv6_type::time_exceeded:
            err = nt_status::HOST_UNREACHABLE;
            break;

        case icmpv6_type::param_problem:
            err = nt_status::PROTOCOL_UNREACHABLE;
            break;

        default:
            unreachable();
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(sock->buffer_lock);

        if (success(sock->pending_error)) {
            sock->pending_error = err;

            if (!sock->signalled) {
                wake_up_waiters(sock->waiters, WAIT_TYPE_READING);
                sock->signalled = true;
            }
        }
    }

    sock->release();
}

nt_status create_new_ipv4_udp_socket(object_ref& ref) {
    auto s = new udp_socket<ipv4>;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

nt_status create_new_ipv6_udp_socket(object_ref& ref) {
    auto s = new udp_socket<ipv6>;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::getsockname(span<uint8_t> buf, size_t& len) {
    if (buf.size() < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(typename FAMILY::sockaddr*)buf.data();

    memset(&addr, 0, sizeof(typename FAMILY::sockaddr));

    if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin_family = FAMILY::family;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        addr.sin6_family = FAMILY::family;

    {
        lock_guard lock(bind_lock);

        if (bind_done) {
            if constexpr (is_same_v<FAMILY, ipv4>) {
                addr.sin_port = bound_port;
                *(ipv4_address*)&addr.sin_addr = bound_address;
            } else if constexpr (is_same_v<FAMILY, ipv6>) {
                addr.sin6_port = bound_port;
                *(ipv6_address*)&addr.sin6_addr = bound_address;
                addr.sin6_scope_id = connected_scope;
            }
        }
    }

    len = sizeof(typename FAMILY::sockaddr);

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::getpeername(span<uint8_t> buf, size_t& len) {
    if (buf.size() < sizeof(typename FAMILY::sockaddr))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(typename FAMILY::sockaddr*)buf.data();

    memset(&addr, 0, sizeof(typename FAMILY::sockaddr));

    if constexpr (is_same_v<FAMILY, ipv4>)
        addr.sin_family = FAMILY::family;
    else if constexpr (is_same_v<FAMILY, ipv6>)
        addr.sin6_family = FAMILY::family;

    {
        lock_guard lock(bind_lock);

        if (!connect_done)
            return nt_status::CONNECTION_DISCONNECTED;

        if constexpr (is_same_v<FAMILY, ipv4>) {
            addr.sin_port = connected_port;
            *(ipv4_address*)&addr.sin_addr = connected_address;
        } else if constexpr (is_same_v<FAMILY, ipv6>) {
            addr.sin6_port = connected_port;
            *(ipv6_address*)&addr.sin6_addr = connected_address;
            addr.sin6_scope_id = connected_scope;
        }
    }

    len = sizeof(typename FAMILY::sockaddr);

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::shutdown(int how) {
    if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(bind_lock);

        if (!connect_done)
            return nt_status::CONNECTION_DISCONNECTED;
    }

    if (how == SHUT_RD || how == SHUT_RDWR) {
        shutdown_read = true;

        {
            sync_lock sl;
            lock_guard lock(sl);

            if (!signalled) {
                wake_up_waiters(waiters, WAIT_TYPE_READING);
                signalled = true;
            }
        }
    }

    if (how == SHUT_WR || how == SHUT_RDWR)
        shutdown_write = true;

    return nt_status::SUCCESS;
}

template<socket_family FAMILY>
nt_status udp_socket<FAMILY>::setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) {
    if constexpr (is_same_v<FAMILY, ipv6>) {
        switch (opt) {
            using enum sockopt_ipv6;

            case V6ONLY: {
                if (buf.size() != sizeof(int))
                    return nt_status::INVALID_PARAMETER;

                // FIXME - do this properly

                return nt_status::SUCCESS;
            }

            default:
                print("FIXME - setsockopt(IPPROTO_IPV6, {}, {})\n", opt, buf.size());
                return nt_status::NOT_SUPPORTED;
        }
    } else
        return nt_status::NOT_SUPPORTED;
}

static uint32_t get_dhcp_xid() {
    auto rnd = (uint64_t)(kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE));

    rnd = rnd * (uint64_t)1103515245 + 12345; // from Knuth

    return (uint32_t)(rnd >> 16);
}

template<size_t N>
struct dhcp_tlv2 : dhcp_tlv {
    uint8_t value[N];
} __attribute((packed));

void send_dhcp_discover(netif& ni) {
    nt_status status;

    static const dhcp_option parameter_list[] = {
        dhcp_option::subnet_mask,
        dhcp_option::router,
        dhcp_option::dns,
        dhcp_option::lease_time,
    };

    struct _discover_msg {
        ipv4_header ip;
        udp_header udp;
        dhcp_message dhcp;
        dhcp_tlv2<1> option_53;
        dhcp_tlv2<sizeof(parameter_list)> option_55;
        dhcp_tlv2<2> option_57;
        dhcp_tlv2<1 + sizeof(mac_address)> option_61;
        dhcp_option option_255;
    } __attribute((packed));

    struct discover_msg : _discover_msg {
        uint8_t padding[300 - sizeof(_discover_msg) + sizeof(ipv4_header) + sizeof(udp_header) ];
    } __attribute((packed));

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& msg = *(discover_msg*)pack->data;
    pack->size = sizeof(discover_msg);

    msg.ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
    msg.ip.version = 4;
    msg.ip.ecn = 0;
    msg.ip.dscp = 0;
    msg.ip.total_length = byteswap((uint16_t)sizeof(discover_msg));
    msg.ip.identification = 0;
    msg.ip.fragment_offset1 = 0;
    msg.ip.MF = 0;
    msg.ip.DF = 1;
    msg.ip.R = 0;
    msg.ip.fragment_offset2 = 0;
    msg.ip.ttl = DEFAULT_HOP_LIMIT;
    msg.ip.protocol = IPPROTO_UDP;
    msg.ip.header_checksum = 0;
    msg.ip.src = parse_ipv4_address("0.0.0.0");
    msg.ip.dst = parse_ipv4_address("255.255.255.255");

    msg.ip.header_checksum = calc_ipv4_checksum(span((uint8_t*)&msg.ip, sizeof(ipv4_header)));

    msg.udp.srcport = byteswap((uint16_t)68);
    msg.udp.dstport = byteswap((uint16_t)67);
    msg.udp.length = byteswap((uint16_t)(sizeof(discover_msg) - sizeof(ipv4_header)));
    msg.udp.checksum = 0;

    memset(&msg.dhcp, 0, sizeof(msg.dhcp));

    msg.dhcp.op = dhcp_opcode::BOOTREQUEST;
    msg.dhcp.htype = 1; // Ethernet
    msg.dhcp.hlen = sizeof(mac_address);
    msg.dhcp.xid = get_dhcp_xid();
    memcpy(msg.dhcp.chaddr, ni.mac.v.data(), sizeof(mac_address));
    msg.dhcp.cookie = DHCP_COOKIE;

    msg.option_53.type = dhcp_option::dhcp_msg;
    msg.option_53.length = 1;
    msg.option_53.value[0] = 1;

    msg.option_55.type = dhcp_option::parameter_list;
    msg.option_55.length = sizeof(parameter_list);
    memcpy(msg.option_55.value, parameter_list, sizeof(parameter_list));

    msg.option_57.type = dhcp_option::dhcp_max_msg_size;
    msg.option_57.length = 2;
    *(uint16_t*)msg.option_57.value = byteswap((uint16_t)MTU);

    msg.option_61.type = dhcp_option::client_id;
    msg.option_61.length = 1 + sizeof(mac_address);
    msg.option_61.value[0] = 1;
    *(mac_address*)&msg.option_61.value[1] = ni.mac;

    msg.option_255 = dhcp_option::end;

    memset(&msg.padding, 0, sizeof(msg.padding));

    msg.udp.checksum = calc_ipv4_checksum(msg.ip, span((uint8_t*)&msg.udp, pack->size - sizeof(ipv4_header)),
                                          IPPROTO_UDP);

    ni.dhcp_status = dhcp_status::sent_discover;

    status = dispatch_packet<ipv4>(ni, *pack, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff });
    if (!success(status)) {
        ni.dhcp_status = dhcp_status::need_discover;
        pack->release();
        return;
    }

    pack->release();
}
