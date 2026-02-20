#include <stdint.h>
#include <stdio.h>
#include <errno.h>

import nt;
import print;
import unique_handle;
import ipv6;
import ws2_32;

using namespace std;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

static void do_ping(int sock, const ipv6_address& ip) {
    icmpv6_echo echo;
    sockaddr_in6 dst;

    dst.sin6_family = AF_INET6;
    dst.sin6_port = 0;
    dst.sin6_flowinfo = 0;
    *(ipv6_address*)&dst.sin6_addr = ip;
    dst.sin6_scope_id = 1; // FIXME - 0 or solicit

    echo.type = icmpv6_type::echo_request;
    echo.code = 0;
    echo.checksum = 0; // done by kernel
    echo.identifier = 0; // FIXME - PID
    echo.sequence = 1; // FIXME - increasing

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

            status = NtWaitForSingleObject(sock, false, &timeout);

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
            auto ms = (float)ticks / (float)SYSTEM_TIME_RESOLUTION;

            sprintf(s, "%.3g", ms);

            // FIXME - add float support to print
            // FIXME - need higher-quality timer?
            print("{} bytes from {}: icmp_seq={} time={} ms\n", len, *(ipv6_address*)&addr.sin6_addr, resp.sequence, s);
        }

        echo.sequence++;
    }

    // FIXME - print summary before quitting
}

int main() {
    auto sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        print_stderr("socket failed (errno {})\n", errno);
        return 1;
    }

    // FIXME - parse cmdline
    // FIXME - if hostname, resolve
    // FIXME - IPv4
    // FIXME - if contains %, find interface name

    // ff02::2
    static const ipv6_address dst = {0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}; // FIXME

    do_ping(sock, dst);

    closesocket(sock);

    return 0;
}
