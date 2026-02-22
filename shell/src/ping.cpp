#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <optional>
#include <bit>
#include <string_view>

import nt;
import print;
import unique_handle;
import ipv6;
import ws2_32;
import moxnox_ioctls;

using namespace std;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

static const char16_t netctl_path[] = u"/dev/netctl";

static void do_ping(int sock, const ipv6_address& ip, uint32_t scope) {
    icmpv6_echo echo;
    sockaddr_in6 dst;
    PROCESS_BASIC_INFORMATION pbi;

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESSINFOCLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print_stderr("NtQueryInformationProcess returned {}\n", status);
        return;
    }

    dst.sin6_family = AF_INET6;
    dst.sin6_port = 0;
    dst.sin6_flowinfo = 0;
    *(ipv6_address*)&dst.sin6_addr = ip;
    dst.sin6_scope_id = scope;

    echo.type = icmpv6_type::echo_request;
    echo.code = 0;
    echo.checksum = 0; // done by kernel
    echo.identifier = byteswap((uint16_t)pbi.UniqueProcessId);
    echo.sequence = byteswap((uint16_t)1);

    char buf[1500]; // FIXME - get MTU
    sockaddr_in6 addr;

    while (true) {
        LARGE_INTEGER sent_time;
        int64_t deadline;
        nt_status status;

        auto ret = sendto(sock, (char*)&echo, sizeof(echo), 0, (sockaddr*)&dst,
                        sizeof(dst));
        if (ret < 0) {
            print_stderr("sendto failed (errno {})\n", errno);
            return; // FIXME - don't quit?
        }

        status = NtQuerySystemTime(&sent_time);
        if (!success(status)) {
            print_stderr("NtQuerySystemTime returned {}\n", status);
            return;
        }

        deadline = sent_time.QuadPart + SYSTEM_TIME_RESOLUTION; // add 1s

        while (true) {
            LARGE_INTEGER now, timeout;
            char s[10];

            status = NtQuerySystemTime(&now);
            if (!success(status)) {
                print_stderr("NtQuerySystemTime returned {}\n", status);
                return;
            }

            if (now.QuadPart >= deadline)
                break;

            timeout.QuadPart = - (deadline - now.QuadPart);

            status = NtWaitForSingleObject((HANDLE)sock, false, &timeout);

            if (status == nt_status::TIMEOUT)
                break;
            else if (!success(status)) {
                print_stderr("NtWaitForSingleObject returned {}\n", status);
                return;
            }

            auto addrlen = (int)sizeof(addr);
            auto len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&addr,
                                &addrlen);
            if (len < 0) {
                print_stderr("recv failed (errno {})\n", errno);
                continue;
            }

            if ((size_t)len < sizeof(icmpv6_echo))
                continue;

            auto& resp = *(icmpv6_echo*)buf;

            if (resp.type != icmpv6_type::echo_reply) // not a ping
                continue;

            if (resp.identifier != echo.identifier) // not our ping
                continue;

            status = NtQuerySystemTime(&now);
            if (!success(status)) {
                print_stderr("NtQuerySystemTime returned {}\n", status);
                return;
            }

            // FIXME - do reverse DNS
            // FIXME - include ttl (need recvmsg)
            // FIXME - check payload

            // FIXME - print if duplicate
            // FIXME - count missing pings etc.

            auto ticks = now.QuadPart - sent_time.QuadPart;
            auto ms = (float)ticks * 1000.0f / (float)SYSTEM_TIME_RESOLUTION;

            sprintf(s, "%.3g", ms);

            // FIXME - add float support to print
            // FIXME - need higher-quality timer?
            print("{} bytes from {}: icmp_seq={} time={} ms\n", len, *(ipv6_address*)&addr.sin6_addr, byteswap(resp.sequence), string_view(s));
        }

        echo.sequence = byteswap((uint16_t)(byteswap(echo.sequence) + 1));
    }

    // FIXME - print summary before quitting
}

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

static uint32_t resolve_scope(string_view s) {
    nt_status status;
    MOXNOX_NETCTL_GET_NETIF_BY_NAME p;
    IO_STATUS_BLOCK iosb;
    uint32_t id;

    if (s.size() > sizeof(p.name))
        return 0;

    auto netctl = open_netctl();
    if (!netctl)
        return 0;

    memcpy(p.name, s.data(), s.size());
    memset(p.name + s.size(), 0, sizeof(p.name) - s.size());

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME, &p,
                                   sizeof(p), &id, (uint32_t)sizeof(id));
    if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return 0;
    }

    return id;
}

int main(int argc, char** argv) {
    // FIXME - cmdline options (getopt?)

    if (argc < 2) {
        print_stderr("ping: usage error: Destination address required\n");
        return 1;
    }

    if (argc > 2) {
        // FIXME - Linux prints usage in this case
        print_stderr("ping: too many cmdline options\n");
        return 1;
    }

    // FIXME - if hostname, resolve
    // FIXME - IPv4
    // FIXME - if contains %, find interface name

    auto dest = string_view(argv[1]);
    string_view scope;
    uint32_t scope_num = 0;

    if (auto pc = dest.find('%'); pc != string_view::npos) {
        scope = string_view(dest.data() + pc + 1, dest.size() - pc - 1);
        dest = string_view(dest.data(), pc);

        if (scope.empty()) {
            print_stderr("ping: no scope given\n");
            return 1;
        }
    }

    auto opt = try_parse_ipv6_address(dest);
    if (!opt.has_value()) {
        print_stderr("ping: could not parse destination \"{}\"\n", dest);
        return 1;
    }

    if (!scope.empty()) {
        scope_num = resolve_scope(scope);
        if (scope_num == 0) {
            print_stderr("ping: could not resolve scope \"{}\"\n", scope);
            return 1;
        }
    }

    auto sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        print_stderr("socket failed (errno {})\n", errno);
        return 1;
    }

    do_ping(sock, *opt, scope_num);

    closesocket(sock);

    return 0;
}
