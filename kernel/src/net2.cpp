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

using namespace std;

static const uint8_t DEFAULT_HOP_LIMIT = 64;

static object_ref netctl_obj;

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

    switch (header.next_header) {
        case ip_proto::icmpv6:
            handle_icmpv6_packet(netif, buf);
            break;

        default:
            break;
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

    // FIXME - validate params
    // FIXME - store params in socket

    auto s = new socket;
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
