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

class ipv6_udp_socket : public socket {
public:
    ~ipv6_udp_socket() override;
    void last_handle_closed() override;

    nt_status recvfrom(span<const span<uint8_t>> v, int flags,
                       sockaddr_storage* from, size_t* fromlen,
                       uint64_t& recv_bytes) override {
        return do_recvfrom<false>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto(span<const span<const uint8_t>> v, int flags, sockaddr* to,
                     size_t tolen, uint64_t& sent_bytes) override;
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
                                uint64_t& recv_bytes) override {
        return do_recvfrom<true>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status shutdown(int how) override;

    nt_status setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) override;

    spinlock buffer_lock;
    list_entry buffer = { &buffer, &buffer };
    size_t buffer_size = 0;
    spinlock bind_lock;
    bool bind_done = false, connect_done = false;
    uint16_t bound_port, connected_port;
    uint32_t connected_scope = 0;
    ipv6_address bound_address, connected_address;
    nt_status pending_error = nt_status::SUCCESS;
    atomic<bool> dead = false, shutdown_read = false, shutdown_write = false;

private:
    template<bool NONBLOCK>
    nt_status do_recvfrom(span<const span<uint8_t>> v, int flags,
                          sockaddr_storage* from, size_t* fromlen,
                          uint64_t& recv_bytes);
};

static spinlock udp_ports_lock;
static ipv6_udp_socket* udp_ports[65536];

struct udp_header {
    uint16_t srcport;
    uint16_t dstport;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

static_assert(sizeof(udp_header) == 8);

static const size_t MAX_BUFFER_SIZE = 1048576;

static void send_port_unreachable(netif& ni, const packet& pack,
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

    ipv6_udp_socket* sock = nullptr;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports[udp.dstport]) {
            sock = udp_ports[udp.dstport];

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
        send_port_unreachable(ni, pack, src_mac);
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
            wake_up_waiters(sock->waiters);
            sock->signalled = true;
        }
    }

    if (!done)
        delete p;

    sock->release();
}

ipv6_udp_socket::~ipv6_udp_socket() {
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

template<bool NONBLOCK>
nt_status ipv6_udp_socket::do_recvfrom(span<const span<uint8_t>> v, int flags,
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

        size_t overhead = sizeof(ipv6_header) + sizeof(udp_header);

        auto to_copy = (uint32_t)min(sp->pack.size - overhead, vec_size);

        memcpy_vec(v, top, sp->pack.data + overhead, to_copy);

        recv_bytes = to_copy;

        if (from && fromlen && *fromlen > 0) {
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

        delete sp;

        break;
    }

    release();

    return nt_status::SUCCESS;
}

static uint16_t find_ephemeral_udp_port() {
    auto rnd = kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE);
    uint16_t port = 49152 + (rnd & 0x3fff);
    uint16_t orig_port = port;

    while (true) {
        if (!udp_ports[byteswap(port)])
            return byteswap(port);

        if (port == 65535)
            port = 49152;
        else
            port++;

        if (port == orig_port)
            return 0;
    }
}

nt_status ipv6_udp_socket::sendto(span<const span<const uint8_t>> v, int flags,
                                  sockaddr* to, size_t tolen, uint64_t& sent_bytes) {
    nt_status status;
    ipv6_address src;
    uint16_t srcport;
    bool connected;
    sockaddr_in6 dest;
    auto vec_size = get_vec_size(v);
    span<const uint8_t> top = vec_size == 0 ? span<const uint8_t>() : v.front();

    {
        lock_guard lock(bind_lock);
        connected = this->connect_done;

        if (connected) {
            *(ipv6_address*)&dest.sin6_addr = connected_address;
            dest.sin6_port = connected_port;
            dest.sin6_scope_id = connected_scope;
        }
    }

    if (!connected && (!to || tolen < sizeof(sockaddr_in6)))
        return nt_status::INVALID_PARAMETER;

    // if connected, sendto destination is ignored
    auto& to6 = connected ? dest : *(sockaddr_in6*)to;

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
            srcport = find_ephemeral_udp_port();
            if (srcport == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;

            udp_ports[srcport] = this;
            this->acquire();

            bound_port = srcport;
            bound_address = parse_ipv6_address("::");

            bind_done = true;
        }

        srcport = bound_port;
        src = bound_address;
    }

    auto& dst = *(ipv6_address*)&to6.sin6_addr;

    netif* pni;

    status = ipv6_choose_interface(dst, to6.sin6_scope_id, pni, src);
    if (!success(status))
        return status;

    auto& ni = *pni;

    if (vec_size + sizeof(ipv6_header) + sizeof(udp_header) > ni.mtu) {
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

    pack->size = sizeof(ipv6_header) + sizeof(udp_header) + vec_size;

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

    auto& udp = *(udp_header*)(pack->data + sizeof(ipv6_header));
    udp.srcport = srcport;
    udp.dstport = to6.sin6_port;
    udp.length = ip.payload_length;
    udp.checksum = 0;

    memcpy_vec(pack->data + sizeof(ipv6_header) + sizeof(udp_header),
               v, top, vec_size);

    if (!ni.tx_udpv6_csum_offload)
        udp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&udp, pack->size - sizeof(ipv6_header)),
                                          IPPROTO_UDP);
    else {
        udp.checksum = calc_ipv6_checksum_offload(ip, (uint32_t)(pack->size - sizeof(ipv6_header)),
                                                  IPPROTO_UDP);
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

    sent_bytes = vec_size;

    return status;
}

nt_status ipv6_udp_socket::bind(const sockaddr* addr, size_t addrlen) {
    if (addr->sa_family != AF_INET6)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(sockaddr_in6))
        return nt_status::BUFFER_TOO_SMALL;

    auto& b = *(sockaddr_in6*)addr;

    auto ip = *(ipv6_address*)&b.sin6_addr;

    if (ip != parse_ipv6_address("::")) {
        if (!is_our_ip(ip))
            return nt_status::INVALID_ADDRESS_COMPONENT;
    }

    {
        uint16_t port;

        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (bind_done)
            return nt_status::INVALID_PARAMETER;

        if (b.sin6_port == 0) {
            port = find_ephemeral_udp_port();
            if (port == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;
        } else {
            port = b.sin6_port;

            if (udp_ports[b.sin6_port])
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;
        }

        udp_ports[b.sin6_port] = this;
        this->acquire();

        bound_port = port;
        bound_address = ip;

        bind_done = true;
    }

    return nt_status::SUCCESS;
}

nt_status ipv6_udp_socket::connect(const sockaddr* addr, size_t addrlen) {
    if (addr->sa_family != AF_INET6)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(sockaddr_in6))
        return nt_status::BUFFER_TOO_SMALL;

    auto& c = *(sockaddr_in6*)addr;

    uint16_t port;

    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (!bind_done) { // implicit bind if not already done
            port = find_ephemeral_udp_port();
            if (port == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;

            udp_ports[port] = this;
            this->acquire();

            bound_port = port;
            bound_address = parse_ipv6_address("::");

            bind_done = true;
        }

        if (connect_done)
            return nt_status::INVALID_PARAMETER;

        connected_port = c.sin6_port;
        connected_address = *(ipv6_address*)&c.sin6_addr;
        connected_scope = c.sin6_scope_id;
        connect_done = true;
    }

    return nt_status::SUCCESS;
}

void ipv6_udp_socket::last_handle_closed() {
    {
        lock_guard lock(udp_ports_lock);
        lock_guard lock2(bind_lock);

        if (bind_done && udp_ports[bound_port] == this) {
            udp_ports[bound_port] = nullptr;
            this->release();
        }
    }

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

void handle_icmpv6_error_udp(packet& pack) {
    const auto& icmp = *(icmpv6_error*)(pack.data + sizeof(ipv6_header));

    if (pack.size < sizeof(ipv6_header) + sizeof(icmpv6_error) + sizeof(udp_header))
        return;

    const auto& udp = *(udp_header*)(pack.data + sizeof(ipv6_header) + sizeof(icmpv6_error));

    ipv6_udp_socket* sock;

    {
        lock_guard lock(udp_ports_lock);

        if (udp_ports[udp.srcport]) {
            sock = udp_ports[udp.srcport];
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
                wake_up_waiters(sock->waiters);
                sock->signalled = true;
            }
        }
    }

    sock->release();
}

nt_status create_new_ipv6_udp_socket(object_ref& ref) {
    auto s = new ipv6_udp_socket;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

nt_status ipv6_udp_socket::getsockname(span<uint8_t> buf, size_t& len) {
    if (buf.size() < sizeof(sockaddr_in6))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(sockaddr_in6*)buf.data();

    memset(&addr, 0, sizeof(sockaddr_in6));

    addr.sin6_family = AF_INET6;

    {
        lock_guard lock(bind_lock);

        if (bind_done) {
            addr.sin6_port = bound_port;
            *(ipv6_address*)&addr.sin6_addr = bound_address;
            addr.sin6_scope_id = connected_scope;
        }
    }

    len = sizeof(sockaddr_in6);

    return nt_status::SUCCESS;
}

nt_status ipv6_udp_socket::getpeername(span<uint8_t> buf, size_t& len) {
    if (buf.size() < sizeof(sockaddr_in6))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(sockaddr_in6*)buf.data();

    memset(&addr, 0, sizeof(sockaddr_in6));

    addr.sin6_family = AF_INET6;

    {
        lock_guard lock(bind_lock);

        if (!connect_done)
            return nt_status::CONNECTION_DISCONNECTED;

        addr.sin6_port = connected_port;
        *(ipv6_address*)&addr.sin6_addr = connected_address;
        addr.sin6_scope_id = connected_scope;
    }

    len = sizeof(sockaddr_in6);

    return nt_status::SUCCESS;
}

nt_status ipv6_udp_socket::shutdown(int how) {
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
                wake_up_waiters(waiters);
                signalled = true;
            }
        }
    }

    if (how == SHUT_WR || how == SHUT_RDWR)
        shutdown_write = true;

    return nt_status::SUCCESS;
}

nt_status ipv6_udp_socket::setsockopt_ipv6(sockopt_ipv6 opt, span<const uint8_t> buf) {
    // FIXME - not if IPv4 socket

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
}
