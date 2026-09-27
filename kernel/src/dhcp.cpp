module;

#include <stdint.h>
#include <stddef.h>
#include <optional>
#include <bit>
#include <algorithm>
#include <span>
#include <mutex>
#include <variant>

module net;

import ipv4;
import print;
import kernel_time_page;
import config;

using namespace std;

template<size_t N>
struct dhcp_tlv2 : dhcp_tlv {
    uint8_t value[N];
} __attribute((packed));

static const dhcp_option dhcp_parameter_list[] = {
    dhcp_option::subnet_mask,
    dhcp_option::router,
    dhcp_option::dns,
    dhcp_option::lease_time,
};

static uint32_t ipv4_address_to_num(const ipv4_address& a) {
    return byteswap(*(uint32_t*)a.v.data());
}

static bool is_subnet_mask_sane(const ipv4_address& subnet_mask) {
    auto v = ipv4_address_to_num(subnet_mask);

    if (subnet_mask == parse_ipv4_address("0.0.0.0"))
        return false;

    if (subnet_mask == parse_ipv4_address("255.255.255.255"))
        return false;

    return countl_one(v) + countr_zero(v) == 32;
}

static void received_dhcp_offer(netif& ni, const ipv4_address& offered_address,
                                const ipv4_address& dhcp_server_id,
                                const optional<ipv4_address>& subnet_mask) {
    nt_status status;

    if (subnet_mask.has_value() && !is_subnet_mask_sane(*subnet_mask))
        return;

    if (offered_address == parse_ipv4_address("0.0.0.0") || offered_address == parse_ipv4_address("255.255.255.255"))
        return;

    if (is_multicast_address(offered_address))
        return;

    if (subnet_mask.has_value()) {
        auto prefix = (uint8_t)countl_one(ipv4_address_to_num(*subnet_mask));

        if (prefix != 31) {
            if (offered_address == calculate_broadcast_address(offered_address, prefix))
                return;

            if ((ipv4_address_to_num(offered_address) & ipv4_address_to_num(*subnet_mask)) == ipv4_address_to_num(offered_address))
                return;
        }
    }

    struct _request_msg {
        ipv4_header ip;
        udp_header udp;
        dhcp_message dhcp;
        dhcp_tlv2<sizeof(ipv4_address)> option_50;
        dhcp_tlv2<1> option_53;
        dhcp_tlv2<sizeof(ipv4_address)> option_54;
        dhcp_tlv2<sizeof(dhcp_parameter_list)> option_55;
        dhcp_tlv2<1 + sizeof(mac_address)> option_61;
        dhcp_option option_255;
    } __attribute((packed));

    struct request_msg : _request_msg {
        uint8_t padding[300 - sizeof(_request_msg) + sizeof(ipv4_header) + sizeof(udp_header)];
    } __attribute((packed));

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& msg = *(request_msg*)pack->data;
    pack->size = sizeof(request_msg);

    msg.ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
    msg.ip.version = 4;
    msg.ip.ecn = 0;
    msg.ip.dscp = 0;
    msg.ip.total_length = byteswap((uint16_t)sizeof(request_msg));
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

    msg.udp.srcport = DHCP_CLIENT_PORT;
    msg.udp.dstport = DHCP_SERVER_PORT;
    msg.udp.length = byteswap((uint16_t)(sizeof(request_msg) - sizeof(ipv4_header)));
    msg.udp.checksum = 0;

    memset(&msg.dhcp, 0, sizeof(msg.dhcp));

    msg.dhcp.op = dhcp_opcode::BOOTREQUEST;
    msg.dhcp.htype = 1; // Ethernet
    msg.dhcp.hlen = sizeof(mac_address);
    msg.dhcp.xid = ni.dhcp_xid;
    memcpy(msg.dhcp.chaddr, ni.mac.v.data(), sizeof(mac_address));
    msg.dhcp.cookie = DHCP_COOKIE;

    msg.option_50.type = dhcp_option::address_request;
    msg.option_50.length = sizeof(ipv4_address);
    *(ipv4_address*)msg.option_50.value = offered_address;

    msg.option_53.type = dhcp_option::dhcp_msg;
    msg.option_53.length = 1;
    msg.option_53.value[0] = (uint8_t)dhcp_message_type::DHCPREQUEST;

    msg.option_54.type = dhcp_option::dhcp_server_id;
    msg.option_54.length = sizeof(ipv4_address);
    *(ipv4_address*)msg.option_54.value = dhcp_server_id;

    msg.option_55.type = dhcp_option::parameter_list;
    msg.option_55.length = sizeof(dhcp_parameter_list);
    memcpy(msg.option_55.value, dhcp_parameter_list, sizeof(dhcp_parameter_list));

    msg.option_61.type = dhcp_option::client_id;
    msg.option_61.length = 1 + sizeof(mac_address);
    msg.option_61.value[0] = 1;
    *(mac_address*)&msg.option_61.value[1] = ni.mac;

    msg.option_255 = dhcp_option::end;

    memset(&msg.padding, 0, sizeof(msg.padding));

    msg.udp.checksum = calc_ipv4_checksum(msg.ip, span((uint8_t*)&msg.udp, pack->size - sizeof(ipv4_header)),
                                          IPPROTO_UDP);

    {
        lock_guard lock(ni.dhcp_lock);

        ni.dhcp_status = dhcp_status::sent_request;
        ni.dhcp_offered_address = offered_address;
        ni.dhcp_server_id = dhcp_server_id;
    }

    status = dispatch_packet<ipv4>(ni, *pack, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff });
    if (!success(status)) {
        {
            lock_guard lock(ni.dhcp_lock);

            if (ni.dhcp_status == dhcp_status::sent_request)
                ni.dhcp_status = dhcp_status::need_discover;
        }

        pack->release();
        return;
    }

    pack->release();
}

static void received_dhcp_ack(netif& ni, const ipv4_address& offered_address,
                              const ipv4_address& dhcp_server_id,
                              const optional<ipv4_address>& subnet_mask,
                              const optional<ipv4_address>& router,
                              optional<uint32_t> lease_time,
                              const optional<ipv4_address>& dns) {
    {
        lock_guard lock(ni.dhcp_lock);

        if (ni.dhcp_status != dhcp_status::done && dhcp_server_id != ni.dhcp_server_id)
            return;

        if (offered_address != ni.dhcp_offered_address)
            return;
    }

    bool already_exists = false;
    uint8_t prefix;

    if (subnet_mask.has_value()) {
        if (!is_subnet_mask_sane(*subnet_mask))
            return;

        prefix = (uint8_t)countl_one(ipv4_address_to_num(*subnet_mask));

        if (prefix != 31) {
            if (offered_address == calculate_broadcast_address(offered_address, prefix))
                return;

            if ((ipv4_address_to_num(offered_address) & ipv4_address_to_num(*subnet_mask)) == ipv4_address_to_num(offered_address))
                return;
        }
    } else {
        if (offered_address.v[0] < 128)
            prefix = 8;
        else if (offered_address.v[0] < 192)
            prefix = 16;
        else
            prefix = 24;
    }

    auto a = new ip_addr(offered_address);
    if (!a) {
        print("new failed\n");
        return;
    }

    a->dhcp = true;
    a->prefix = prefix;

    {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a2 = containing_record<ip_addr>(le);

            if (auto* addr = get_if<ipv4_address>(&a2.addr)) {
                if (*addr == offered_address) {
                    already_exists = true;
                    break;
                }
            }
        }

        if (!already_exists)
            insert_tail_list(ni.addrs, a->le);
    }

    if (already_exists)
        delete a;

    if (!lease_time.has_value() || *lease_time == 0)
        lease_time = 43200; // 12 hours

    // FIXME - option for this
    auto renew_time = max(*lease_time / 2, (uint32_t)1);

    // FIXME - option for this too
    auto rebind_time = max(*lease_time - (*lease_time / 8), (uint32_t)1);

    {
        lock_guard lock(ni.dhcp_lock);

        ni.dhcp_status = dhcp_status::done;
        ni.dhcp_lease_left = *lease_time;
        ni.dhcp_renew_time = renew_time;
        ni.dhcp_rebind_time = rebind_time;
        ni.dhcp_server_id = dhcp_server_id;
    }

    print("{}: added new IPv4 address {}\n", ni, offered_address);

    add_ipv4_route(ni, zero_right_bits(offered_address, 32 - prefix), prefix,
                   parse_ipv4_address("0.0.0.0"), true);

    if (router.has_value()) {
        if (zero_right_bits(*router, 32 - prefix) == zero_right_bits(offered_address, 32 - prefix))
            add_ipv4_route(ni, parse_ipv4_address("0.0.0.0"), 0, *router, true);
    }

    if (dns.has_value()) {
        auto addr = parse_ipv6_address("::ffff:0:0");

        memcpy(addr.v.data() + 12, dns->v.data(), sizeof(ipv4_address));

        add_dns_server(addr, *lease_time);
    }
}

static void received_dhcp_nak(netif& ni, const ipv4_address& dhcp_server_id) {
    {
        lock_guard lock(ni.dhcp_lock);

        if (dhcp_server_id != ni.dhcp_server_id)
            return;

        // FIXME - should be waiting 10 seconds before sending DISCOVER again

        ni.dhcp_status = dhcp_status::need_discover;
    }

    handle_dhcp_expiry(ni);
}

void received_dhcp_packet(netif& ni, packet& pack) {
    auto& ip = *(ipv4_header*)pack.data;
    auto ip_header_size = ip.ihl * sizeof(uint32_t);
    auto& udp = *(udp_header*)(pack.data + ip_header_size);

    if (byteswap(udp.length) < sizeof(udp_header) + sizeof(dhcp_message))
        return;

    auto& dhcp = *(dhcp_message*)((uint8_t*)&udp + sizeof(udp_header));

    if (dhcp.cookie != DHCP_COOKIE)
        return;

    dhcp_status cur_status;

    {
        lock_guard lock(ni.dhcp_lock);

        if (dhcp.xid != ni.dhcp_xid)
            return;

        cur_status = ni.dhcp_status;
    }

    if (dhcp.op != dhcp_opcode::BOOTREPLY || dhcp.htype != 1 || dhcp.hlen != sizeof(mac_address))
        return;

    auto opts = span((uint8_t*)&dhcp + sizeof(dhcp_message),
                     byteswap(udp.length) - sizeof(udp_header) - sizeof(dhcp_message));

#if 0
    print("DHCP message: op = {}, htype = {}, hlen = {}, hops = {}, xid = {}, secs = {}, flags = {}, ciaddr = {}, yiaddr = {}, siaddr = {}, giaddr = {}, chaddr = {}\n",
          (uint8_t)dhcp.op, dhcp.htype, dhcp.hlen, dhcp.hops, +dhcp.xid,
          +dhcp.secs, +dhcp.flags, dhcp.ciaddr, dhcp.yiaddr, dhcp.siaddr,
          dhcp.giaddr, *(mac_address*)dhcp.chaddr);
#endif

    optional<ipv4_address> subnet_mask, router, dns, dhcp_server_id;
    optional<uint32_t> lease_time;
    optional<dhcp_message_type> dhcp_msg;

    while (!opts.empty()) {
        auto opt = (dhcp_option)opts[0];

        if (opt == dhcp_option::end)
            break;

        if ((uint8_t)opt == 0) {
            opts = span(opts.data() + 1, opts.size() - 1);
            continue;
        }

        if (opts.size() < sizeof(dhcp_tlv))
            break;

        auto& tlv = *(dhcp_tlv*)opts.data();

        opts = span(opts.data() + sizeof(dhcp_tlv), opts.size() - sizeof(dhcp_tlv));

        if (opts.size() < tlv.length)
            break;

        auto default_message = [&]() {
#if 0
            print("DHCP option: type {}, length {}\n", tlv.type, tlv.length);
#endif
        };

        switch (tlv.type) {
            case dhcp_option::subnet_mask:
                if (tlv.length >= sizeof(ipv4_address)) {
#if 0
                    print("DHCP option {}: {}\n", tlv.type, *(ipv4_address*)opts.data());
#endif
                    subnet_mask = *(ipv4_address*)opts.data();
                } else
                    default_message();
                break;

            case dhcp_option::router:
                if (tlv.length >= sizeof(ipv4_address)) {
#if 0
                    print("DHCP option {}: {}\n", tlv.type, *(ipv4_address*)opts.data());
#endif
                    router = *(ipv4_address*)opts.data();
                } else
                    default_message();
                break;

            case dhcp_option::dns:
                // FIXME - can have multiple servers
                if (tlv.length >= sizeof(ipv4_address)) {
#if 0
                    print("DHCP option {}: {}\n", tlv.type, *(ipv4_address*)opts.data());
#endif
                    dns = *(ipv4_address*)opts.data();
                } else
                    default_message();
                break;

            case dhcp_option::lease_time:
                if (tlv.length >= sizeof(uint32_t)) {
#if 0
                    print("DHCP option {}: {}\n", tlv.type, byteswap(*(uint32_t*)opts.data()));
#endif
                    lease_time = byteswap(*(uint32_t*)opts.data());
                } else
                    default_message();
                break;

            case dhcp_option::dhcp_msg:
                if (tlv.length >= sizeof(dhcp_message_type)) {
#if 0
                    print("DHCP option {}: {}\n", tlv.type, *(dhcp_message_type*)opts.data());
#endif
                    dhcp_msg = *(dhcp_message_type*)opts.data();
                } else
                    default_message();
                break;

            case dhcp_option::dhcp_server_id:
                if (tlv.length >= sizeof(ipv4_address)) {
#if 0
                    print("DHCP option {}: {}\n", tlv.type, *(ipv4_address*)opts.data());
#endif
                    dhcp_server_id = *(ipv4_address*)opts.data();
                } else
                    default_message();
                break;

            default:
                default_message();
                break;
        }

        opts = span(opts.data() + tlv.length, opts.size() - tlv.length);
    }

    if (!dhcp_msg.has_value())
        return;

    switch (cur_status) {
        using enum dhcp_status;

        case sent_discover:
            if (*dhcp_msg == dhcp_message_type::DHCPOFFER && dhcp_server_id.has_value())
                received_dhcp_offer(ni, dhcp.yiaddr, *dhcp_server_id, subnet_mask);
        break;

        case sent_request:
        case done:
            if (*dhcp_msg == dhcp_message_type::DHCPACK && dhcp_server_id.has_value()) {
                received_dhcp_ack(ni, dhcp.yiaddr, *dhcp_server_id,
                                  subnet_mask, router, lease_time, dns);
            } else if (*dhcp_msg == dhcp_message_type::DHCPNAK && dhcp_server_id.has_value())
                received_dhcp_nak(ni, *dhcp_server_id);
        break;

        default:
            break;
    }
}

static uint32_t get_dhcp_xid() {
    auto rnd = (uint64_t)(kernel_time_page.system_time / (SYSTEM_TIME_RESOLUTION / TICK_RATE));

    rnd = rnd * (uint64_t)1103515245 + 12345; // from Knuth

    return (uint32_t)(rnd >> 16);
}

void send_dhcp_discover(netif& ni) {
    nt_status status;

    struct _discover_msg {
        ipv4_header ip;
        udp_header udp;
        dhcp_message dhcp;
        dhcp_tlv2<1> option_53;
        dhcp_tlv2<sizeof(dhcp_parameter_list)> option_55;
        dhcp_tlv2<2> option_57;
        dhcp_tlv2<1 + sizeof(mac_address)> option_61;
        dhcp_option option_255;
    } __attribute((packed));

    struct discover_msg : _discover_msg {
        uint8_t padding[300 - sizeof(_discover_msg) + sizeof(ipv4_header) + sizeof(udp_header)];
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

    msg.udp.srcport = DHCP_CLIENT_PORT;
    msg.udp.dstport = DHCP_SERVER_PORT;
    msg.udp.length = byteswap((uint16_t)(sizeof(discover_msg) - sizeof(ipv4_header)));
    msg.udp.checksum = 0;

    memset(&msg.dhcp, 0, sizeof(msg.dhcp));

    auto xid = get_dhcp_xid();

    msg.dhcp.op = dhcp_opcode::BOOTREQUEST;
    msg.dhcp.htype = 1; // Ethernet
    msg.dhcp.hlen = sizeof(mac_address);
    msg.dhcp.xid = xid;
    memcpy(msg.dhcp.chaddr, ni.mac.v.data(), sizeof(mac_address));
    msg.dhcp.cookie = DHCP_COOKIE;

    msg.option_53.type = dhcp_option::dhcp_msg;
    msg.option_53.length = 1;
    msg.option_53.value[0] = (uint8_t)dhcp_message_type::DHCPDISCOVER;

    msg.option_55.type = dhcp_option::parameter_list;
    msg.option_55.length = sizeof(dhcp_parameter_list);
    memcpy(msg.option_55.value, dhcp_parameter_list, sizeof(dhcp_parameter_list));

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

    bool status_changed = false;

    {
        lock_guard lock(ni.dhcp_lock);

        if (ni.dhcp_status != dhcp_status::need_discover)
            status_changed = true;
        else {
            ni.dhcp_status = dhcp_status::sent_discover;
            ni.dhcp_xid = xid;
        }
    }

    if (status_changed) {
        pack->release();
        return;
    }

    status = dispatch_packet<ipv4>(ni, *pack, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff });
    if (!success(status)) {
        {
            lock_guard lock(ni.dhcp_lock);

            if (ni.dhcp_status == dhcp_status::sent_discover)
                ni.dhcp_status = dhcp_status::need_discover;
        }

        pack->release();
        return;
    }

    pack->release();
}

void handle_dhcp_expiry(netif& ni) {
    list_entry removing = { &removing, &removing };

    {
        lock_guard lock(ni.addr_lock);

        for (auto le = ni.addrs.next; le != &ni.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            if (get_if<ipv4_address>(&a.addr)) {
                if (a.dhcp) {
                    le = le->prev;
                    remove_entry_list(a.le);
                    insert_tail_list(removing, a.le);
                }
            }
        }
    }

    while (!is_list_empty(removing)) {
        auto& a = containing_record<ip_addr>(removing.next);

        remove_entry_list(a.le);
        delete &a;
    }

    {
        lock_guard lock(ipv4_routes_lock);

        for (auto le = ipv4_routes.next; le != &ipv4_routes; le = le->next) {
            auto& r = containing_record<ipv4_route>(le);

            if (&r.ni == &ni && r.dhcp) {
                le = le->prev;
                remove_entry_list(r.le);
                insert_tail_list(removing, r.le);
            }
        }
    }

    while (!is_list_empty(removing)) {
        auto& r = containing_record<ipv4_route>(removing.next);

        remove_entry_list(r.le);
        delete &r;
    }

    {
        lock_guard lock(ni.dhcp_lock);

        if (ni.dhcp_status == dhcp_status::done)
            ni.dhcp_status = dhcp_status::need_discover;
    }
}

void do_dhcp_renewal(netif& ni, bool broadcast) {
    nt_status status;

    struct _request_msg {
        ipv4_header ip;
        udp_header udp;
        dhcp_message dhcp;
        dhcp_tlv2<1> option_53;
        dhcp_tlv2<sizeof(dhcp_parameter_list)> option_55;
        dhcp_tlv2<1 + sizeof(mac_address)> option_61;
        dhcp_option option_255;
    } __attribute((packed));

    struct request_msg : _request_msg {
        uint8_t padding[300 - sizeof(_request_msg) + sizeof(ipv4_header) + sizeof(udp_header)];
    } __attribute((packed));

    ipv4_address source_address, dhcp_server_id, gateway;
    auto xid = get_dhcp_xid();
    mac_address dst_mac;

    {
        lock_guard lock(ni.dhcp_lock);

        source_address = ni.dhcp_offered_address;
        dhcp_server_id = ni.dhcp_server_id;
    }

    if (broadcast)
        dst_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    else {
        netif* pni;

        status = ipv4_choose_interface(dhcp_server_id, pni, source_address, gateway);
        if (!success(status))
            return;
        else if (pni != &ni) {
            pni->release();
            return;
        }

        pni->release();

        // FIXME - can block housekeeping thread for a few seconds
        dst_mac = find_mac_address(ni, gateway == parse_ipv4_address("0.0.0.0") ? dhcp_server_id : gateway);

        if (dst_mac == NULL_MAC_ADDRESS)
            return;
    }

    auto pack = new packet;
    if (!pack) {
        print("new failed\n");
        return;
    }

    auto& msg = *(request_msg*)pack->data;
    pack->size = sizeof(request_msg);

    msg.ip.ihl = sizeof(ipv4_header) / sizeof(uint32_t);
    msg.ip.version = 4;
    msg.ip.ecn = 0;
    msg.ip.dscp = 0;
    msg.ip.total_length = byteswap((uint16_t)sizeof(request_msg));
    msg.ip.identification = 0;
    msg.ip.fragment_offset1 = 0;
    msg.ip.MF = 0;
    msg.ip.DF = 1;
    msg.ip.R = 0;
    msg.ip.fragment_offset2 = 0;
    msg.ip.ttl = DEFAULT_HOP_LIMIT;
    msg.ip.protocol = IPPROTO_UDP;
    msg.ip.header_checksum = 0;
    msg.ip.src = source_address;
    msg.ip.dst = broadcast ? parse_ipv4_address("255.255.255.255") : dhcp_server_id;

    msg.ip.header_checksum = calc_ipv4_checksum(span((uint8_t*)&msg.ip, sizeof(ipv4_header)));

    msg.udp.srcport = DHCP_CLIENT_PORT;
    msg.udp.dstport = DHCP_SERVER_PORT;
    msg.udp.length = byteswap((uint16_t)(sizeof(request_msg) - sizeof(ipv4_header)));
    msg.udp.checksum = 0;

    memset(&msg.dhcp, 0, sizeof(msg.dhcp));

    msg.dhcp.op = dhcp_opcode::BOOTREQUEST;
    msg.dhcp.htype = 1; // Ethernet
    msg.dhcp.hlen = sizeof(mac_address);
    msg.dhcp.xid = xid;
    msg.dhcp.ciaddr = source_address;
    memcpy(msg.dhcp.chaddr, ni.mac.v.data(), sizeof(mac_address));
    msg.dhcp.cookie = DHCP_COOKIE;

    msg.option_53.type = dhcp_option::dhcp_msg;
    msg.option_53.length = 1;
    msg.option_53.value[0] = (uint8_t)dhcp_message_type::DHCPREQUEST;

    msg.option_55.type = dhcp_option::parameter_list;
    msg.option_55.length = sizeof(dhcp_parameter_list);
    memcpy(msg.option_55.value, dhcp_parameter_list, sizeof(dhcp_parameter_list));

    msg.option_61.type = dhcp_option::client_id;
    msg.option_61.length = 1 + sizeof(mac_address);
    msg.option_61.value[0] = 1;
    *(mac_address*)&msg.option_61.value[1] = ni.mac;

    msg.option_255 = dhcp_option::end;

    memset(&msg.padding, 0, sizeof(msg.padding));

    msg.udp.checksum = calc_ipv4_checksum(msg.ip, span((uint8_t*)&msg.udp, pack->size - sizeof(ipv4_header)),
                                          IPPROTO_UDP);

    {
        lock_guard lock(ni.dhcp_lock);
        ni.dhcp_xid = xid;
    }

    status = dispatch_packet<ipv4>(ni, *pack, dst_mac);
    if (!success(status)) {
        {
            lock_guard lock(ni.dhcp_lock);

            if (ni.dhcp_status == dhcp_status::done)
                ni.dhcp_status = dhcp_status::need_discover;
        }

        handle_dhcp_expiry(ni);

        pack->release();
        return;
    }

    pack->release();
}
