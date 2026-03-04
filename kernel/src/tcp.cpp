module;

#include <atomic>
#include <span>
#include <optional>
#include <mutex>
#include <bit>
#include <algorithm>
#include <type_traits>
#include <stdlib.h>

module net;

import print;
import thread;
import event;
import scheduler;
import kernel_time_page;
import list;
import config;

using namespace std;

static const uint64_t RETRANSMISSION_DELAY = 1; // seconds
static const uint64_t TIME_WAIT_LENGTH = 60; // seconds

struct ipv6_tcp_retransmission {
    ipv6_tcp_retransmission(packet& pack) : pack(pack) {
        pack.acquire();
    }

    ~ipv6_tcp_retransmission() {
        pack.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    list_entry le;
    atomic<unsigned int> refcount = 1;
    uint64_t deadline;
    unsigned int attempt = 0;
    packet& pack;
};

class tcp_seq {
public:
    constexpr tcp_seq() = default;

    explicit constexpr tcp_seq(uint32_t v) {
        data[0] = (uint8_t)((v >> 24) & 0xff);
        data[1] = (v >> 16) & 0xff;
        data[2] = (v >> 8) & 0xff;
        data[3] = v & 0xff;
    }

    constexpr uint32_t host() const noexcept {
        return (static_cast<uint32_t>(data[0]) << 24) |
                (static_cast<uint32_t>(data[1]) << 16) |
                (static_cast<uint32_t>(data[2]) << 8) |
                static_cast<uint32_t>(data[3]);
    }

    constexpr tcp_seq& operator+=(uint32_t n) {
        *this = tcp_seq(host() + n);
        return *this;
    }

    constexpr tcp_seq operator++(int) {
        tcp_seq old = *this;
        *this = tcp_seq(host() + 1);
        return old;
    }

    constexpr tcp_seq operator+(uint32_t n) const {
        return tcp_seq(host() + n);
    }

    constexpr int32_t operator-(tcp_seq other) const {
        return static_cast<int32_t>(host() - other.host());
    }

    constexpr strong_ordering operator<=>(tcp_seq other) const {
        int32_t diff = static_cast<int32_t>(host() - other.host());

        return diff <=> 0;
    }

    constexpr bool operator==(const tcp_seq& other) const {
        return !memcmp(data, other.data, sizeof(data));
    }

private:
    uint8_t data[4];
};

static_assert(is_standard_layout_v<tcp_seq>);
static_assert(is_trivial_v<tcp_seq>);

class ipv6_tcp_socket : public socket {
public:
    ipv6_tcp_socket();
    ~ipv6_tcp_socket();

    void last_handle_closed() override;

    nt_status recvfrom(char* buf, size_t len, int flags, sockaddr* from,
                       int* fromlen, size_t& recv_bytes) override;
    nt_status sendto(const char* buf, size_t len, int flags, sockaddr* to,
                     int tolen, size_t& sent_bytes) override;
    nt_status bind(const sockaddr* addr, size_t addrlen) override;
    nt_status connect(const sockaddr* addr, size_t addrlen) override;

    void received_packet(packet& pack);

    bool init = false;
    spinlock state_lock;
    object_ref state_change_event;
    bool bind_done = false;
    uint16_t bound_port, connected_port;
    uint32_t connected_scope;
    ipv6_address bound_address, connected_address;
    tcp_state state = tcp_state::CLOSED;
    netif* pni = nullptr;
    uint16_t recv_window = 0xffff;
    tcp_seq send_next, send_unacknowledged, recv_next;
    uint16_t send_window = 0;
    tcp_seq last_send_window_seq;
    mac_address dst_mac;
    packet* first_ack = nullptr;
    size_t mss = 1220; // FIXME - different for IPv4
    spinlock retransmission_list_lock;
    list_entry retransmission_list = { &retransmission_list, &retransmission_list };
    object_ref retransmission_thread_obj;
    object_ref retransmission_thread_quit_event;
    object_ref send_window_open_event;
    list_entry buffer = { &buffer, &buffer };
    list_entry ooo_buffer = { &ooo_buffer, &ooo_buffer };
    rwsem us_read_mutex;
    size_t us_read_offset = 0;
    rwsem io_mutex;

private:
    nt_status send_syn(const sockaddr_in6& c, ipv6_address& src);
    void send_ack(bool first, tcp_seq ack);
    void send_fin();
    void retransmission_thread();
    void do_retransmissions();
};

static spinlock tcp_ports_lock;
static ipv6_tcp_socket* tcp_ports[65536];

struct tcp_header {
    uint16_t src_port;
    uint16_t dst_port;
    tcp_seq seq;
    tcp_seq ack;
    uint8_t : 4;
    uint8_t header_length : 4;
    union {
        struct {
            uint8_t FIN : 1;
            uint8_t SYN : 1;
            uint8_t RST : 1;
            uint8_t PSH : 1;
            uint8_t ACK : 1;
            uint8_t URG : 1;
            uint8_t ECE : 1;
            uint8_t CWR : 1;
        };
        uint8_t flags;
    };
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
};

static_assert(sizeof(tcp_header) == 20);

static const uint8_t TCP_SYN_ACK = 0x12;

static void send_tcp_port_closed(netif& ni, const ipv6_address& src,
                                 uint16_t src_port, const ipv6_address& dst,
                                 uint16_t dst_port, const mac_address& mac,
                                 tcp_seq seq) {
    auto pack2 = new packet;
    if (!pack2) {
        print("new failed\n");
        return;
    }

    pack2->size = sizeof(ipv6_header) + sizeof(tcp_header);

    auto& ip = *(ipv6_header*)pack2->data;
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label = 0;
    ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
    ip.next_header = IPPROTO_TCP;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = dst; // FIXME - make sure not multicast?
    ip.dst = src;

    auto& tcp = *(tcp_header*)(pack2->data + sizeof(ipv6_header));

    memset(&tcp, 0, sizeof(tcp));

    tcp.src_port = dst_port;
    tcp.dst_port = src_port;
    tcp.ack = seq + 1;
    tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
    tcp.RST = 1;
    tcp.ACK = 1;

    tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack2->size - sizeof(ipv6_header)), IPPROTO_TCP);

    auto status = ni.send_packet(mac, ethertype::ipv6, *pack2);
    if (!success(status))
        print("netif::send_packet returned {}\n", status);

    pack2->release();
}

void received_ipv6_tcp_packet(netif& ni, packet& pack,
                              const mac_address& src_mac) {
    if (pack.size < sizeof(ipv6_header) + sizeof(tcp_header))
        return;

    auto& ip = *(ipv6_header*)pack.data;

    if (is_multicast_address(ip.dst))
        return;

    auto& tcp = *(tcp_header*)(pack.data + sizeof(ipv6_header));

    if (calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack.size - sizeof(ipv6_header)), IPPROTO_TCP) != 0)
        return;

#if 0
    print("received_tcp_packet (src {}, dst {}, {:04x})\n",
          byteswap(tcp.src_port), byteswap(tcp.dst_port), byteswap(tcp.checksum));
#endif

    ipv6_tcp_socket* sock = nullptr;

    {
        lock_guard lock(tcp_ports_lock);

        if (tcp_ports[tcp.dst_port]) {
            sock = tcp_ports[tcp.dst_port];

            {
                lock_guard lock(sock->state_lock);

                if (!sock->bind_done || (sock->bound_address != parse_ipv6_address("::") && sock->bound_address != ip.dst))
                    sock = nullptr;
                else
                    sock->acquire();
            }
        }
    }

    if (!sock) {
        if (!tcp.RST) {
            send_tcp_port_closed(ni, ip.src, tcp.src_port, ip.dst,
                                 tcp.dst_port, src_mac, tcp.seq);
        }
        return;
    }

    sock->received_packet(pack);

    sock->release();
}

static uint16_t find_ephemeral_tcp_port() {
    auto rnd = kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE);
    uint16_t port = 49152 + (rnd & 0x3fff);
    uint16_t orig_port = port;

    while (true) {
        if (!tcp_ports[byteswap(port)])
            return byteswap(port);

        if (port == 65535)
            port = 49152;
        else
            port++;

        if (port == orig_port)
            return 0;
    }
}

nt_status ipv6_tcp_socket::send_syn(const sockaddr_in6& c, ipv6_address& src) {
    nt_status status;

    auto& dst = *(ipv6_address*)c.sin6_addr;

    {
        netif* newni;

        status = ipv6_choose_interface(dst, c.sin6_scope_id, newni, src);
        if (!success(status))
            return status;

        {
            lock_guard lock(state_lock);

            swap(pni, newni);
        }

        if (newni)
            newni->release();
    }

    {
        lock_guard lock(state_lock);

        if (bound_address == parse_ipv6_address("::"))
            bound_address = src;
    }

    auto& ni = *pni;

    if (src == dst)
        dst_mac = ni.mac;
    else
        dst_mac = find_mac_address(ni, dst);

    ni.acquire();

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    pack->size = sizeof(ipv6_header) + sizeof(tcp_header);

    auto& ip = *(ipv6_header*)pack->data;
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label = 0;
    ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
    ip.next_header = IPPROTO_TCP;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = src;
    ip.dst = dst;

    auto& tcp = *(tcp_header*)(pack->data + sizeof(ipv6_header));

    memset(&tcp, 0, sizeof(tcp));

    tcp.src_port = bound_port;
    tcp.dst_port = connected_port;
    tcp.seq = send_next;
    tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
    tcp.SYN = 1;
    tcp.window = byteswap(recv_window);

    tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)), IPPROTO_TCP);

    if (dst_mac == NULL_MAC_ADDRESS) {
        send_address_unreachable(ni, *pack, src);
        pack->release();
        ni.release();
        return nt_status::HOST_UNREACHABLE;
    }

    auto& ev = static_cast<event&>(state_change_event.get());

    for (unsigned int i = 0; i < 3; i++) {
        status = dispatch_packet(ni, *pack, dst_mac);
        if (!success(status)) {
            pack->release();
            ni.release();
            return status;
        }

        status = wait_for_object(ev, true, SYSTEM_TIME_RESOLUTION); // wait 1 second

        if (status == nt_status::TIMEOUT)
            continue;
        else if (success(status))
            break;
        else {
            pack->release();
            ni.release();
            return status;
        }
    }

    pack->release();
    ni.release();

    {
        lock_guard lock(state_lock);

        if (state == tcp_state::CLOSED)
            return nt_status::CONNECTION_REFUSED;
    }

    if (status == nt_status::TIMEOUT)
        return nt_status::HOST_UNREACHABLE;

    return nt_status::SUCCESS;
}

static tcp_seq rand_tcp_seq() {
    auto rnd = (uint64_t)(kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE));

    rnd = rnd * (uint64_t)1103515245 + 12345; // from Knuth

    return tcp_seq{(uint32_t)(rnd >> 16)};
}

nt_status ipv6_tcp_socket::connect(const sockaddr* addr, size_t addrlen) {
    nt_status status;

    if (addr->sa_family != AF_INET6)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < sizeof(sockaddr_in6))
        return nt_status::BUFFER_TOO_SMALL;

    auto& c = *(sockaddr_in6*)addr;
    ipv6_address src;

    status = nt_status::SUCCESS;

    {
        uint16_t port;
        lock_guard lock(tcp_ports_lock);
        lock_guard lock2(state_lock);

        if (state != tcp_state::CLOSED)
            return nt_status::INVALID_PARAMETER;

        if (!bind_done) { // implicit bind if not already done
            port = find_ephemeral_tcp_port();
            if (port == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;

            tcp_ports[port] = this;
            this->acquire();

            bound_port = port;
            bound_address = parse_ipv6_address("::");

            bind_done = true;
        }

        state = tcp_state::SYN_SENT;

        connected_port = c.sin6_port;
        connected_address = *(ipv6_address*)c.sin6_addr;
        connected_scope = c.sin6_scope_id;

        src = bound_address;

        send_next = rand_tcp_seq();
        send_unacknowledged = send_next;
    }

    status = send_syn(c, src);
    if (!success(status)) {
        netif* oldni = nullptr;

        // FIXME - should also undo bind?

        {
            lock_guard lock(state_lock);
            state = tcp_state::CLOSED;
            swap(oldni, pni);
        }

        if (oldni)
            oldni->release();

        return status;
    }

    return nt_status::SUCCESS;
}

void ipv6_tcp_socket::last_handle_closed() {
    acquire();

    add_kernel_thread([](void* ctx) {
        auto& s = *(ipv6_tcp_socket*)ctx;

        bool do_fin = false;

        {
            lock_guard lock(s.state_lock);

            if (s.state == tcp_state::ESTABLISHED) {
                s.state = tcp_state::FIN_WAIT_1;
                do_fin = true;
            } else if (s.state == tcp_state::CLOSE_WAIT) {
                s.state = tcp_state::LAST_ACK;
                do_fin = true;
            }
        }

        if (do_fin)
            s.send_fin();

        s.release();
    }, this);
}

void ipv6_tcp_socket::send_ack(bool first, tcp_seq ack) {
    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& ni = *pni;

    ni.acquire();

    pack->size = sizeof(ipv6_header) + sizeof(tcp_header);

    auto& ip = *(ipv6_header*)pack->data;
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label = 0;
    ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
    ip.next_header = IPPROTO_TCP;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = bound_address;
    ip.dst = connected_address;

    auto& tcp = *(tcp_header*)(pack->data + sizeof(ipv6_header));

    memset(&tcp, 0, sizeof(tcp));

    {
        lock_guard lock(io_mutex);

        if (first)
            send_next++;

        tcp.src_port = bound_port;
        tcp.dst_port = connected_port;
        tcp.seq = send_next;
        tcp.ack = ack;
        tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
        tcp.ACK = 1;
        tcp.window = byteswap(recv_window);

        tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)), IPPROTO_TCP);

        dispatch_packet(ni, *pack, dst_mac);

        if (first) {
            pack->acquire();
            first_ack = pack;
        } else if (first_ack) {
            first_ack->release();
            first_ack = nullptr;
        }
    }

    pack->release();
    ni.release();
}

void ipv6_tcp_socket::send_fin() {
    // FIXME - send RST if new fails?

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto r = new ipv6_tcp_retransmission(*pack);
    if (!r) {
        print("new failed\n");
        pack->release();
        return;
    }

    auto& ni = *pni;

    ni.acquire();

    pack->size = sizeof(ipv6_header) + sizeof(tcp_header);

    auto& ip = *(ipv6_header*)pack->data;
    ip.traffic_class1 = 0;
    ip.version = 6;
    ip.traffic_class2 = 0;
    ip.flow_label = 0;
    ip.payload_length = byteswap((uint16_t)sizeof(tcp_header));
    ip.next_header = IPPROTO_TCP;
    ip.hop_limit = DEFAULT_HOP_LIMIT;
    ip.src = bound_address;
    ip.dst = connected_address;

    auto& tcp = *(tcp_header*)(pack->data + sizeof(ipv6_header));

    memset(&tcp, 0, sizeof(tcp));

    {
        lock_guard lock(io_mutex);

        tcp.src_port = bound_port;
        tcp.dst_port = connected_port;
        tcp.seq = send_next;
        tcp.ack = recv_next;
        tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
        tcp.FIN = 1;
        tcp.ACK = 1;
        tcp.window = byteswap(recv_window);

        send_next++;

        tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)), IPPROTO_TCP);

        r->deadline = (uint64_t)kernel_time_page.system_time + (RETRANSMISSION_DELAY * SYSTEM_TIME_RESOLUTION);

        {
            lock_guard lock(retransmission_list_lock);
            insert_tail_list(retransmission_list, r->le);
        }

        dispatch_packet(ni, *pack, dst_mac);
    }

    pack->release();
    ni.release();
}

void ipv6_tcp_socket::received_packet(packet& pack) {
    auto& ip = *(ipv6_header*)pack.data;
    auto& tcp = *(tcp_header*)(pack.data + sizeof(ipv6_header));
    size_t header_length = tcp.header_length * sizeof(uint32_t);

    if (header_length < sizeof(tcp_header))
        return;

    if (pack.size < sizeof(ipv6_header) + header_length)
        return;

    tcp_state cur_state;

    {
        lock_guard lock(state_lock);

        if (state == tcp_state::CLOSED)
            return;

        if (connected_address != ip.src || connected_port != tcp.src_port)
            return;

        cur_state = state;
    }

    if (tcp.RST) {
        {
            lock_guard lock(state_lock);
            state = tcp_state::CLOSED;
        }

        set_event(static_cast<event&>(state_change_event.get()));
        return;
    }

    {
        lock_guard lock(io_mutex);

        if ((cur_state == tcp_state::SYN_SENT && tcp.flags == TCP_SYN_ACK) || tcp.seq >= last_send_window_seq) {
            last_send_window_seq = tcp.seq;

            if (byteswap(tcp.window) == 0 && send_window > 0)
                reset_event(static_cast<event&>(send_window_open_event.get()));
            else if (byteswap(tcp.window) > 0 && send_window == 0)
                set_event(static_cast<event&>(send_window_open_event.get()));

            send_window = byteswap(tcp.window);
        }
    }

    if (cur_state == tcp_state::SYN_SENT && tcp.flags == TCP_SYN_ACK) {
        tcp_seq ack;

        // FIXME - parse options

        {
            lock_guard lock(state_lock);

            recv_next = tcp.seq + 1;
            send_unacknowledged = tcp.ack;

            ack = recv_next;
        }

        send_ack(true, ack);

        {
            lock_guard lock(state_lock);
            state = cur_state = tcp_state::ESTABLISHED;
        }

        set_event(static_cast<event&>(state_change_event.get()));
    } else if (cur_state == tcp_state::ESTABLISHED && tcp.flags == TCP_SYN_ACK) {
        // first ACK must have been lost, retransmit
        if (first_ack)
            dispatch_packet(*pni, *first_ack, dst_mac);
    }

    if (tcp.ACK) {
        bool do_cleanup = false;
        auto ack = tcp.ack;

        while (true) {
            ipv6_tcp_retransmission* r = nullptr;

            {
                lock_guard lock(retransmission_list_lock);

                if (is_list_empty(retransmission_list)) {
                    {
                        lock_guard lock(state_lock);

                        if (state == tcp_state::FIN_WAIT_1)
                            state = tcp_state::FIN_WAIT_2;
                        else if (state == tcp_state::LAST_ACK)
                            do_cleanup = true;
                    }
                    break;
                }

                auto& r2 = containing_record<ipv6_tcp_retransmission>(retransmission_list.next);

                auto& rtcp = *(tcp_header*)(r2.pack.data + sizeof(ipv6_header));

                if (ack <= rtcp.seq)
                    break;

                send_unacknowledged = ack;

                remove_entry_list(r2.le);
                r = &r2;
            }

            r->release();
        }

        if (do_cleanup) {
            {
                lock_guard lock(tcp_ports_lock);
                lock_guard lock2(state_lock);

                if (bind_done && tcp_ports[bound_port] == this) {
                    tcp_ports[bound_port] = nullptr;
                    release();
                }
            }

            set_event(static_cast<event&>(retransmission_thread_quit_event.get()));
        }
    }

    if (pack.size > header_length + sizeof(ipv6_header) || tcp.FIN) { // data or FIN
        auto p = new ipv6_socket_packet(pack);
        if (!p) {
            print("new failed\n");
            return;
        }

        p->scope_id = connected_scope;

        bool added = false;
        optional<tcp_seq> ack;
        bool clear_ooo = false;

        {
            lock_guard lock(io_mutex);
            lock_guard lock2(state_lock);

            auto seq = tcp.seq;

            if (seq > recv_next) {
                auto data_len = (uint32_t)(pack.size - header_length - sizeof(ipv6_header));

                if (data_len <= recv_window) {
                    recv_window -= (uint16_t)data_len;

                    insert_tail_list(ooo_buffer, p->le);
                    added = true;
                }
            } else if (seq < recv_next) {
                // send ACK for packet we've already received
                ack = recv_next;
            } else {
                if (pack.size > header_length + sizeof(ipv6_header)) {
                    insert_tail_list(buffer, p->le);
                    added = true;
                }

                auto data_len = (uint32_t)(pack.size - header_length - sizeof(ipv6_header));

                recv_next += data_len;

                if (tcp.FIN && data_len <= recv_window) {
                    recv_next++;

                    if (state == tcp_state::ESTABLISHED)
                        state = tcp_state::CLOSE_WAIT;
                    else if (state == tcp_state::FIN_WAIT_2)
                        state = tcp_state::TIME_WAIT;

                    clear_ooo = true;
                } else {
                    // check out-of-order list for anything now okay
                    while (true) {
                        if (is_list_empty(ooo_buffer))
                            break;

                        auto& p2 = containing_record<ipv6_socket_packet>(ooo_buffer.next);
                        auto& tcp2 = *(tcp_header*)(p2.pack.data + sizeof(ipv6_header));
                        size_t header_length2 = tcp2.header_length * sizeof(uint32_t);

                        // FIXME - what if tcp2.seq < recv_next (i.e. packets overlap)?

                        if (tcp2.seq != recv_next)
                            break;

                        recv_next += (uint32_t)(p2.pack.size - header_length2 - sizeof(ipv6_header));

                        if (tcp2.FIN) {
                            recv_next++;

                            if (state == tcp_state::ESTABLISHED)
                                state = tcp_state::CLOSE_WAIT;
                            else if (state == tcp_state::FIN_WAIT_2)
                                state = tcp_state::TIME_WAIT;

                            clear_ooo = true;
                            break;
                        } else {
                            remove_entry_list(p2.le);
                            insert_tail_list(buffer, p2.le);
                        }
                    }
                }

                if (data_len > recv_window)
                    data_len = recv_window;

                ack = recv_next;
                recv_window -= (uint16_t)data_len;
            }
        }

        if (clear_ooo) {
            while (true) {
                ipv6_socket_packet* p;

                {
                    lock_guard lock(state_lock);

                    if (!is_list_empty(ooo_buffer))
                        p = &containing_record<ipv6_socket_packet>(ooo_buffer.next);
                    else
                        break;

                    remove_entry_list(p->le);
                }

                delete p;
            }
        }

        if (!added)
            delete p;

        if (ack.has_value()) {
            send_ack(false, *ack);

            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(state_lock);

                if (!is_list_empty(buffer) || state != tcp_state::ESTABLISHED) {
                    if (!signalled) {
                        wake_up_waiters(waiters);
                        signalled = true;
                    }
                } else
                    signalled = false;
            }
        }
    }
}

nt_status ipv6_tcp_socket::sendto(const char* buf, size_t len, int flags,
                                  sockaddr*, int, size_t& sent_bytes) {
    nt_status status;

    // sockaddr is ignored

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(state_lock);

        if (state != tcp_state::ESTABLISHED && state != tcp_state::CLOSE_WAIT)
            return nt_status::CONNECTION_DISCONNECTED;
    }

    // FIXME - Nagle's algorithm (batching of outgoing data)
    // FIXME - windowing

    size_t done = 0;

    auto& ni = *pni;

    ni.acquire();

    while (len > 0) {
        size_t size;

        status = wait_for_object(send_window_open_event.get(), true);
        if (!success(status))
            break;

        {
            lock_guard lock(io_mutex);

            size = min(len, mss);
            auto usable = (uint16_t)((send_unacknowledged - send_next) + send_window);

            if (size > usable)
                size = usable;

            if (size == 0)
                continue;

            auto pack = new packet;
            if (!pack) {
                print("new failed\n");
                ni.release();

                if (done == 0)
                    return nt_status::INSUFFICIENT_RESOURCES;
                else {
                    sent_bytes = done;
                    return nt_status::SUCCESS;
                }
            }

            auto r = new ipv6_tcp_retransmission(*pack);
            if (!r) {
                print("new failed\n");
                pack->release();
                ni.release();

                if (done == 0)
                    return nt_status::INSUFFICIENT_RESOURCES;
                else {
                    sent_bytes = done;
                    return nt_status::SUCCESS;
                }
            }

            pack->size = sizeof(ipv6_header) + sizeof(tcp_header) + size;

            auto& ip = *(ipv6_header*)pack->data;
            ip.traffic_class1 = 0;
            ip.version = 6;
            ip.traffic_class2 = 0;
            ip.flow_label = 0;
            ip.payload_length = byteswap((uint16_t)(sizeof(tcp_header) + size));
            ip.next_header = IPPROTO_TCP;
            ip.hop_limit = DEFAULT_HOP_LIMIT;
            ip.src = bound_address;
            ip.dst = connected_address;

            uint32_t read_bytes;

            status = t.proc->read_vm(buf, pack->data + sizeof(ipv6_header) + sizeof(tcp_header),
                                    (uint32_t)size, &read_bytes);

            if (success(status) && read_bytes < size) {
                status = nt_status::NO_MEMORY;
                print("process::read_vm returned {} bytes, expected {}\n",
                    read_bytes, size);
            } else if (!success(status))
                print("process::read_vm returned {}\n", status);

            if (!success(status)) {
                pack->release();
                delete r;
                ni.release();

                if (done == 0)
                    return status;
                else {
                    sent_bytes = done;
                    return nt_status::SUCCESS;
                }
            }

            auto& tcp = *(tcp_header*)(pack->data + sizeof(ipv6_header));

            memset(&tcp, 0, sizeof(tcp));

            tcp.src_port = bound_port;
            tcp.dst_port = connected_port;
            tcp.seq = send_next;
            tcp.ack = recv_next;
            tcp.header_length = sizeof(tcp_header) / sizeof(uint32_t);
            tcp.PSH = 1;
            tcp.ACK = 1;
            tcp.window = byteswap(recv_window);

            send_next += (uint32_t)size;

            tcp.checksum = calc_ipv6_checksum(ip, span((uint8_t*)&tcp, pack->size - sizeof(ipv6_header)), IPPROTO_TCP);

            dispatch_packet(ni, *pack, dst_mac);

            r->deadline = (uint64_t)kernel_time_page.system_time + (RETRANSMISSION_DELAY * SYSTEM_TIME_RESOLUTION);

            {
                lock_guard lock(retransmission_list_lock);

                insert_tail_list(retransmission_list, r->le);
            }

            pack->release();
        }

        done += size;
        buf += size;
        len -= size;
    }

    ni.release();

    sent_bytes = done;

    return status;
}

ipv6_tcp_socket::ipv6_tcp_socket() {
    state_change_event = create_event(true);
    if (!state_change_event) {
        print("create_event failed\n");
        return;
    }

    retransmission_thread_quit_event = create_event(false);
    if (!retransmission_thread_quit_event) {
        print("create_event failed\n");
        return;
    }

    send_window_open_event = create_event(false);
    if (!send_window_open_event) {
        print("create_event failed\n");
        return;
    }

    this->acquire();

    retransmission_thread_obj = add_kernel_thread([](void* ctx) {
        auto& s = *(ipv6_tcp_socket*)ctx;

        s.retransmission_thread();

        s.release();
    }, this);

    init = true;
}

ipv6_tcp_socket::~ipv6_tcp_socket() {
    while (true) {
        ipv6_tcp_retransmission* r;

        {
            lock_guard lock(retransmission_list_lock);

            if (is_list_empty(retransmission_list))
                break;

            r = &containing_record<ipv6_tcp_retransmission>(retransmission_list.next);

            remove_entry_list(r->le);
        }

        r->release();
    }

    while (true) {
        ipv6_socket_packet* p;

        {
            lock_guard lock(state_lock);

            if (!is_list_empty(buffer))
                p = &containing_record<ipv6_socket_packet>(buffer.next);
            else if (!is_list_empty(ooo_buffer))
                p = &containing_record<ipv6_socket_packet>(ooo_buffer.next);
            else
                break;

            remove_entry_list(p->le);
        }

        delete p;
    }

    if (pni)
        pni->release();
}

void ipv6_tcp_socket::retransmission_thread() {
    unsigned int time_wait_count = 0;

    auto& ev = static_cast<event&>(retransmission_thread_quit_event.get());

    while (true) {
        bool time_wait_done = false;

        auto status = wait_for_object(ev, true, SYSTEM_TIME_RESOLUTION);

        if (status == nt_status::SUCCESS)
            break;

        if (!success(status)) {
            print("wait_for_object returned {}, exiting network thread\n", status);
            break;
        }

        {
            lock_guard lock(state_lock);

            if (state == tcp_state::TIME_WAIT) {
                time_wait_count++;
                if (time_wait_count == TIME_WAIT_LENGTH)
                    time_wait_done = true;
            }
        }

        if (time_wait_done) {
            {
                lock_guard lock(tcp_ports_lock);
                lock_guard lock2(state_lock);

                if (bind_done && tcp_ports[bound_port] == this) {
                    tcp_ports[bound_port] = nullptr;
                    release();
                }
            }

            break;
        }

        do_retransmissions();
    }
}

void ipv6_tcp_socket::do_retransmissions() {
    unsigned int num = 0, added = 0;
    auto now = (uint64_t)kernel_time_page.system_time;

    {
        lock_guard lock(retransmission_list_lock);

        for (auto le = retransmission_list.next; le != &retransmission_list; le = le->next) {
            auto& r = containing_record<ipv6_tcp_retransmission>(le);

            if (now >= r.deadline)
                num++;
            else
                break;
        }
    }

    if (num == 0)
        return;

    auto arr = (ipv6_tcp_retransmission**)malloc(sizeof(ipv6_tcp_retransmission*) * num);
    if (!arr) {
        print("malloc failed\n");
        return;
    }

    {
        lock_guard lock(retransmission_list_lock);

        for (auto le = retransmission_list.next; le != &retransmission_list; le = le->next) {
            auto& r = containing_record<ipv6_tcp_retransmission>(le);

            if (now >= r.deadline) {
                r.acquire();
                arr[added] = &r;
                added++;

                if (added == num)
                    break;
            } else
                break;
        }
    }

    for (unsigned int i = 0; i < added; i++) {
        auto& r = *arr[i];

        dispatch_packet(*pni, r.pack, dst_mac);

        // FIXME - delay increase should be exponential

        r.attempt++;
        r.deadline = (uint64_t)kernel_time_page.system_time + (RETRANSMISSION_DELAY * SYSTEM_TIME_RESOLUTION);

        r.release();
    }

    free(arr);
}

nt_status ipv6_tcp_socket::recvfrom(char* buf, size_t len, int flags,
                                    sockaddr* from, int* fromlen,
                                    size_t& recv_bytes) {
    nt_status status;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    auto& t = *get_current_thread();

    recv_bytes = 0;

    acquire();

    while (true) {
        lock_guard lock(us_read_mutex);

        // FIXME - break lock if last handle to socket closed
        status = wait_for_object(*this, true);
        if (!success(status)) {
            release();
            return status;
        }

        size_t removed_bytes = 0;

        while (true) {
            ipv6_socket_packet* r;

            {
                lock_guard lock(state_lock);

                if (is_list_empty(buffer))
                    break;

                r = &containing_record<ipv6_socket_packet>(buffer.next);
            }

            auto& tcp = *(tcp_header*)(r->pack.data + sizeof(ipv6_header));

            size_t header_length = tcp.header_length * sizeof(uint32_t);
            auto to_copy = (uint32_t)min(r->pack.size - sizeof(ipv6_header) - header_length - us_read_offset, len);
            uint32_t written_bytes;

            status = t.proc->write_vm(buf, r->pack.data + sizeof(ipv6_header) + header_length + us_read_offset,
                                      to_copy, &written_bytes);
            if (!success(status)) {
                print("process::write_vm returned {}\n", status);
                break;
            }

            if (written_bytes != to_copy) {
                print("process::write_vm: wrote {} bytes, expected {}\n",
                      written_bytes, to_copy);
                status = nt_status::NO_MEMORY;
                break;
            }

            recv_bytes += to_copy;

            if (sizeof(ipv6_header) + header_length + us_read_offset + to_copy < r->pack.size) {
                us_read_offset += to_copy;
                break;
            }

            {
                lock_guard lock(state_lock);
                remove_entry_list(r->le);
            }

            buf += to_copy;
            len -= to_copy;
            us_read_offset = 0;
            removed_bytes += r->pack.size - sizeof(ipv6_header) - header_length;

            delete r;

            if (len == 0)
                break;
        }

        if (removed_bytes != 0) {
            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(state_lock);

                if (is_list_empty(buffer) && state == tcp_state::ESTABLISHED)
                    signalled = false;
            }

            bool opening = false;
            tcp_seq ack;

            {
                lock_guard lock(io_mutex);

                if (recv_window == 0)
                    opening = true;

                recv_window += (uint16_t)removed_bytes;
                ack = recv_next;
            }

            if (opening)
                send_ack(false, ack);
        }

        if (!success(status) && recv_bytes == 0) {
            release();
            return status;
        }

        if (from && fromlen && *fromlen > 0) {
            sockaddr_in6 addr;

            memset(&addr, 0, sizeof(addr));

            addr.sin6_family = AF_INET6;
            addr.sin6_port = connected_port;
            addr.sin6_flowinfo = 0; // FIXME? Allowed by RFC3493, but not necessarily what Linux does
            *(ipv6_address*)addr.sin6_addr = connected_address;
            addr.sin6_scope_id = connected_scope;

            status = t.proc->write_vm(from, &addr,
                                      (uint32_t)min((size_t)*fromlen, sizeof(addr)),
                                      nullptr);
            if (!success(status)) {
                release();
                return status;
            }

            *fromlen = sizeof(addr);
        }

        break;
    }

    release();

    return nt_status::SUCCESS;
}

nt_status ipv6_tcp_socket::bind(const sockaddr* addr, size_t addrlen) {
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
        uint16_t port;
        lock_guard lock(tcp_ports_lock);
        lock_guard lock2(state_lock);

        if (bind_done)
            return nt_status::INVALID_PARAMETER;

        if (b.sin6_port == 0) {
            port = find_ephemeral_tcp_port();
            if (port == 0) // no ephemeral ports left
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;
        } else {
            port = b.sin6_port;

            if (tcp_ports[b.sin6_port])
                return nt_status::ADDRESS_ALREADY_ASSOCIATED;
        }

        tcp_ports[port] = this;
        this->acquire();

        bound_port = port;
        bound_address = ip;

        bind_done = true;
    }

    return nt_status::SUCCESS;
}

nt_status create_new_ipv6_tcp_socket(object_ref& ref) {
    auto s = new ipv6_tcp_socket;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!s->init) {
        s->release();
        return nt_status::INTERNAL_ERROR;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}
