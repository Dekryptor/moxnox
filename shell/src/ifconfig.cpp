#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <optional>
#include <string_view>

import nt;
import print;
import format;
import unique_handle;
import moxnox_ioctls;
import ipv4;
import ipv6;
import timepage;

using namespace std;

static const char16_t netctl_path[] = u"/dev/netctl";

static unique_handle open_netctl() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return {};
    }

    return unique_handle{h};
}

static optional<uint32_t> get_netif_count(unique_handle& netctl) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    MOXNOX_NETCTL_GET_NETIF_COUNT out;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_COUNT, nullptr,
                                   0, &out, sizeof(out));
    if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return nullopt;
    }

    return out.count;
}

static void get_netif_info(unique_handle& netctl, uint32_t id) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    MOXNOX_NETCTL_GET_NETIF_INFO info;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_INFO, &id,
                                   sizeof(uint32_t), &info,
                                   (uint32_t)sizeof(info));
    if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return;
    }

    auto name = string_view(info.name, sizeof(info.name));
    if (auto n = name.find('\0'); n != string_view::npos)
        name = string_view(name.data(), n);

    print("{}:\n", name);
    print("    RX packets {} bytes {} ({})\n", info.rx_packets, info.rx_bytes,
          byte_size(info.rx_bytes));
    print("    TX packets {} bytes {} ({})\n", info.tx_packets, info.tx_bytes,
          byte_size(info.tx_bytes));
}

static void get_netif_ips(unique_handle& netctl, uint32_t id) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    uint32_t count;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_IP_COUNT, &id,
                                   sizeof(uint32_t), &count,
                                   (uint32_t)sizeof(count));
    if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return;
    }

    if (count == 0)
        return;

    size_t size = offsetof(MOXNOX_NETCTL_GET_NETIF_IPS, ip) + (count * sizeof(ipv6_address));
    auto ips = (MOXNOX_NETCTL_GET_NETIF_IPS*)malloc(size);
    if (!ips) {
        print("malloc failed\n");
        return;
    }

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_IPS, &id,
                                   sizeof(uint32_t), ips,
                                   (uint32_t)size);
    if (!success(status)) {
        free(ips);
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return;
    }

    static const uint8_t MAPPED_IPV4[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff };

    for (uint32_t i = 0; i < ips->count; i++) {
        if (!memcmp(ips->ip[i].v.data(), MAPPED_IPV4, sizeof(MAPPED_IPV4))) {
            ipv4_address addr;

            addr.v[0] = ips->ip[i].v[12];
            addr.v[1] = ips->ip[i].v[13];
            addr.v[2] = ips->ip[i].v[14];
            addr.v[3] = ips->ip[i].v[15];

            print("    IPv4 address: {}\n", addr);
        } else
            print("    IPv6 address: {}\n", ips->ip[i]);
    }

    free(ips);
}

int main() {
    auto netctl = open_netctl();

    if (!netctl)
        return 1;

    uint32_t count;

    if (auto opt = get_netif_count(netctl); !opt.has_value())
        return 1;
    else
        count = *opt;

    if (count == 0)
        return 0;

    // FIXME - get count again if we get nt_status::BUFFER_TOO_SMALL?

    size_t ids_size = offsetof(MOXNOX_NETCTL_GET_NETIF_IDS, id) + (count * sizeof(uint32_t));
    auto ids = (MOXNOX_NETCTL_GET_NETIF_IDS*)malloc(ids_size);
    if (!ids) {
        print_stderr("malloc failed\n");
        return 1;
    }

    nt_status status;
    IO_STATUS_BLOCK iosb;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_IDS, nullptr,
                                   0, ids, (uint32_t)ids_size);
    if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        free(ids);
        return 1;
    }

    for (unsigned int i = 0; i < ids->count; i++) {
        if (i != 0)
            print("\n");

        get_netif_info(netctl, ids->id[i]);
        get_netif_ips(netctl, ids->id[i]);
    }

    free(ids);

    print("\nDNS servers:\n");

    auto dns = get_dns_servers();

    if (dns[0] == parse_ipv6_address("::"))
        print("(none)\n");
    else {
        for (auto& d : dns) {
            if (d == parse_ipv6_address("::"))
                break;

            static const auto MAPPED_ADDRESS = parse_ipv6_address("::ffff:0:0");

            if (!memcmp(d.v.data(), MAPPED_ADDRESS.v.data(), 12))
                print("{}\n", *(ipv4_address*)(d.v.data() + 12));
            else
                print("{}\n", d);
        }
    }
}
