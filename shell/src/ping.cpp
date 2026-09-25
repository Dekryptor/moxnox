#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <optional>
#include <span>
#include <bit>
#include <string_view>

import nt;
import print;
import ipv4;
import ipv6;
import ws2_32;
import ucrtbase;

#undef errno
import errno;

using namespace std;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

static void handle_echo_reply_v4(span<const char> data, int64_t sent_time,
                                 const sockaddr_in& addr, uint16_t identifier) {
    char s[10], host[NI_MAXHOST];
    nt_status status;
    LARGE_INTEGER now;

    if (data.size() < sizeof(icmpv4_echo))
        return;

    auto& resp = *(icmpv4_echo*)data.data();

    if (resp.identifier != identifier) // not our ping
        return;

    status = NtQuerySystemTime(&now);
    if (!success(status)) {
        print_stderr("NtQuerySystemTime returned {}\n", status);
        return;
    }

    // FIXME - include ttl (need recvmsg)
    // FIXME - check payload

    // FIXME - print if duplicate
    // FIXME - count missing pings etc.

    auto ticks = now.QuadPart - sent_time;
    auto ms = (float)ticks * 1000.0f / (float)SYSTEM_TIME_RESOLUTION;

    sprintf(s, "%.3g", ms);

    auto ret = getnameinfo((sockaddr*)&addr, sizeof(sockaddr_in), host,
                           sizeof(host), nullptr, 0, NI_NAMEREQD);

    if (!ret) {
        print("{} bytes from {} ({}): icmp_seq={} time={} ms\n", data.size(),
              host, *(ipv4_address*)&addr.sin_addr, byteswap(resp.sequence),
              string_view(s));
    } else {
        print("{} bytes from {}: icmp_seq={} time={} ms\n", data.size(),
              *(ipv4_address*)&addr.sin_addr, byteswap(resp.sequence),
              string_view(s));
    }
}

static void handle_echo_reply_v6(span<const char> data, int64_t sent_time,
                                 const sockaddr_in6& addr, uint16_t identifier) {
    char s[10], host[NI_MAXHOST];
    nt_status status;
    LARGE_INTEGER now;

    if (data.size() < sizeof(icmpv6_echo))
        return;

    auto& resp = *(icmpv6_echo*)data.data();

    if (resp.identifier != identifier) // not our ping
        return;

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

    auto ticks = now.QuadPart - sent_time;
    auto ms = (float)ticks * 1000.0f / (float)SYSTEM_TIME_RESOLUTION;

    sprintf(s, "%.3g", ms);

    auto ret = getnameinfo((sockaddr*)&addr, sizeof(sockaddr_in6), host, sizeof(host),
                           nullptr, 0, NI_NAMEREQD);

    // FIXME - add float support to print
    // FIXME - need higher-quality timer?
    if (!ret) {
        print("{} bytes from {} ({}): icmp_seq={} time={} ms\n", data.size(),
              host, *(ipv6_address*)&addr.sin6_addr, byteswap(resp.sequence),
              string_view(s));
    } else {
        print("{} bytes from {}: icmp_seq={} time={} ms\n", data.size(),
              *(ipv6_address*)&addr.sin6_addr, byteswap(resp.sequence),
              string_view(s));
    }
}

static void handle_dest_unreachable_v6(span<const char> data, const sockaddr_in6& addr,
                                       uint16_t identifier) {
    if (data.size() < sizeof(icmpv6_error))
        return;

    auto& resp = *(icmpv6_error*)data.data();

    if (resp.header.next_header != IPPROTO_ICMPV6)
        return;

    if (data.size() < sizeof(icmpv6_error) + sizeof(icmpv6_header))
        return;

    auto& payload = *(icmpv6_header*)(data.data() + sizeof(icmpv6_error));

    if (payload.type != icmpv6_type::echo_request) // not a ping
        return;

    if (data.size() < sizeof(icmpv6_error) + sizeof(icmpv6_echo))
        return;

    auto& echo = *(icmpv6_echo*)(data.data() + sizeof(icmpv6_error));

    if (echo.identifier != identifier) // not our ping
        return;

    string_view message, reason;

    // FIXME - print codes if unknown

    switch (resp.type) {
        case icmpv6_type::dest_unreachable:
            message = "Destination unreachable: ";

            switch ((icmpv6_dest_unreachable_code)resp.code) {
                using enum icmpv6_dest_unreachable_code;

                case no_route:
                    reason = "No route";
                    break;

                case administratively_prohibited:
                    reason = "Administratively prohibited";
                    break;

                case beyond_scope:
                    reason = "Beyond scope of source address";
                    break;

                case address_unreachable:
                    reason = "Address unreachable";
                    break;

                case no_port:
                    reason = "Port unreachable";
                    break;

                case policy_fail:
                    reason = "Source address failed ingress/egress policy";
                    break;

                case reject_route:
                    reason = "Reject route to destination";
                    break;

                default:
                    reason = "?";
                    break;
            }
        break;

        case icmpv6_type::packet_too_big:
            message = "Packet too big"; // FIXME - include mtu, like Linux ping does
            reason = "";
            // FIXME - include code if not 0
        break;

        case icmpv6_type::time_exceeded:
            message = "Time exceeded: ";

            switch ((icmpv6_time_exceeded_code)resp.code) {
                using enum icmpv6_time_exceeded_code;

                case hop_limit:
                    reason = "Hop limit";
                    break;

                case defragmentation_failure:
                    reason = "Defragmentation failure";
                    break;

                default:
                    reason = "?";
                    break;
            }
        break;

        case icmpv6_type::param_problem:
            message = "Parameter problem: ";

            switch ((icmpv6_param_problem_code)resp.code) {
                using enum icmpv6_param_problem_code;

                case wrong_header_field:
                    reason = "Wrong header field";
                    break;

                case unknown_header:
                    reason = "Unknown header";
                    break;

                case unknown_option:
                    reason = "Unknown option";
                    break;

                default:
                    reason = "?";
                    break;
            }

            // FIXME - Linux ping then has "at" value
        break;

        default:
            break;
    }

    print_stderr("From {} icmp_seq={} {}{}\n",
                 *(ipv6_address*)&addr.sin6_addr, byteswap(echo.sequence),
                 message, reason);
}

static void do_ping_v6(SOCKET sock, const addrinfo& ai, string_view arg) {
    icmpv6_echo echo;
    PROCESS_BASIC_INFORMATION pbi;

    auto& sa = *(sockaddr_in6*)ai.ai_addr;
    auto& dest = *(ipv6_address*)&sa.sin6_addr;

    print("PING {} ({}) 0 data bytes\n", arg, dest);

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESS_INFORMATION_CLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print_stderr("NtQueryInformationProcess returned {}\n", status);
        return;
    }

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

        auto ret = sendto(sock, (char*)&echo, sizeof(echo), 0, ai.ai_addr,
                          (int)ai.ai_addrlen);
        if (ret < 0)
            print_stderr("sendto failed (errno {})\n", errno_to_string_view(*_errno()));

        status = NtQuerySystemTime(&sent_time);
        if (!success(status)) {
            print_stderr("NtQuerySystemTime returned {}\n", status);
            return;
        }

        deadline = sent_time.QuadPart + SYSTEM_TIME_RESOLUTION; // add 1s

        while (true) {
            LARGE_INTEGER now;
            pollfd pfd;

            status = NtQuerySystemTime(&now);
            if (!success(status)) {
                print_stderr("NtQuerySystemTime returned {}\n", status);
                return;
            }

            if (now.QuadPart >= deadline)
                break;

            pfd.fd = (int)sock;
            pfd.events = POLLIN;

            ret = poll(&pfd, 1, (int)((deadline - now.QuadPart) / 10000)); // convert units of 100ns to ms

            if (ret == 0)
                break;
            else if (ret < 0) {
                print_stderr("poll failed (errno {})\n", errno_to_string_view(*_errno()));
                return;
            }

            auto addrlen = (int)sizeof(addr);
            auto len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&addr,
                                &addrlen);
            if (len < 0) {
                print_stderr("recv failed (errno {})\n", errno_to_string_view(*_errno()));
                continue;
            }

            if ((size_t)len < sizeof(icmpv6_header))
                continue;

            auto& h = *(icmpv6_header*)buf;

            switch (h.type) {
                case icmpv6_type::dest_unreachable:
                case icmpv6_type::packet_too_big:
                case icmpv6_type::time_exceeded:
                case icmpv6_type::param_problem:
                    handle_dest_unreachable_v6(span(buf, (size_t)len), addr,
                                               echo.identifier);
                break;

                case icmpv6_type::echo_reply:
                    handle_echo_reply_v6(span(buf, (size_t)len), sent_time.QuadPart,
                                         addr, echo.identifier);
                break;

                default:
                    break;
            }
        }

        echo.sequence = byteswap((uint16_t)(byteswap(echo.sequence) + 1));
    }

    // FIXME - print summary before quitting
}

static void handle_dest_unreachable_v4(span<const char> data, const sockaddr_in& addr,
                                       uint16_t identifier) {
    if (data.size() < sizeof(icmpv4_error))
        return;

    auto& resp = *(icmpv4_error*)data.data();

    if (resp.header.protocol != IPPROTO_ICMP)
        return;

    if (data.size() < sizeof(icmpv4_error))
        return;

    auto& payload = *(icmpv4_header*)(data.data() + sizeof(icmpv4_error));

    if (payload.type != icmpv4_type::echo_request) // not a ping
        return;

    if (data.size() < sizeof(icmpv4_error) + sizeof(icmpv4_echo))
        return;

    auto& echo = *(icmpv4_echo*)(data.data() + sizeof(icmpv4_error));

    if (echo.identifier != identifier) // not our ping
        return;

    string_view message, reason;

    // FIXME - print codes if unknown

    switch (resp.type) {
        case icmpv4_type::dest_unreachable:
            message = "Destination unreachable: ";

            switch ((icmpv4_dest_unreachable_code)resp.code) {
                using enum icmpv4_dest_unreachable_code;

                case dest_net_unreachable:
                    reason = "Destination Net Unreachable";
                    break;

                case dest_host_unreachable:
                    reason = "Destination Host Unreachable";
                    break;

                case dest_proto_unreachable:
                    reason = "Destination Protocol Unreachable";
                    break;

                case dest_port_unreachable:
                    reason = "Destination Port Unreachable";
                    break;

                case fragmentation_required:
                    reason = "Frag needed and DF set"; // FIXME - Linux includes MTU here
                    break;

                case src_route_failed:
                    reason = "Source Route Failed";
                    break;

                case dest_net_unknown:
                    reason = "Destination Net Unknown";
                    break;

                case dest_host_unknown:
                    reason = "Destination Host Unknown";
                    break;

                case src_host_isolated:
                    reason = "Source Host Isolated";
                    break;

                case net_administratively_prohibited:
                    reason = "Destination Net Prohibited";
                    break;

                case host_administratively_prohibited:
                    reason = "Destination Host Prohibited";
                    break;

                case net_unreachable_for_tos:
                    reason = "Destination Net Unreachable for Type of Service";
                    break;

                case host_unreachable_for_tos:
                    reason = "Destination Host Unreachable for Type of Service";
                    break;

                case communication_administratively_prohibited:
                    reason = "Packet filtered";
                    break;

                case host_precedence_violation:
                    reason = "Precedence Violation";
                    break;

                case precedence_cutoff_in_effect:
                    reason = "Precedence Cutoff";
                    break;

                default:
                    reason = "?";
                    break;
            }
        break;

        case icmpv4_type::time_exceeded:
            message = "Time exceeded: ";

            switch ((icmpv4_time_exceeded_code)resp.code) {
                using enum icmpv4_time_exceeded_code;

                case ttl_expired:
                    reason = "Time to live exceeded";
                    break;

                case fragmentation_time_exceeded:
                    reason = "Frag reassembly time exceeded";
                    break;

                default:
                    reason = "?";
                    break;
            }
        break;

        case icmpv4_type::param_problem:
            message = "Parameter problem";
        break;

        default:
            break;
    }

    print_stderr("From {} icmp_seq={} {}{}\n",
                 *(ipv4_address*)&addr.sin_addr, byteswap(echo.sequence),
                 message, reason);
}

static void do_ping_v4(SOCKET sock, const addrinfo& ai, string_view arg) {
    icmpv4_echo echo;
    PROCESS_BASIC_INFORMATION pbi;

    auto& sa = *(sockaddr_in*)ai.ai_addr;
    auto& dest = *(ipv4_address*)&sa.sin_addr;

    print("PING {} ({}) 0 data bytes\n", arg, dest);

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESS_INFORMATION_CLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print_stderr("NtQueryInformationProcess returned {}\n", status);
        return;
    }

    echo.type = icmpv4_type::echo_request;
    echo.code = 0;
    echo.identifier = byteswap((uint16_t)pbi.UniqueProcessId);
    echo.sequence = byteswap((uint16_t)1);

    char buf[1500]; // FIXME - get MTU
    sockaddr_in addr;

    while (true) {
        LARGE_INTEGER sent_time;
        int64_t deadline;
        nt_status status;
        int ret;

        echo.checksum = 0;
        echo.checksum = calc_ipv4_checksum(span((uint8_t*)&echo, sizeof(echo)));

        ret = sendto(sock, (char*)&echo, sizeof(echo), 0, ai.ai_addr,
                     (int)ai.ai_addrlen);
        if (ret < 0)
            print_stderr("sendto failed (errno {})\n", errno_to_string_view(*_errno()));

        status = NtQuerySystemTime(&sent_time);
        if (!success(status)) {
            print_stderr("NtQuerySystemTime returned {}\n", status);
            return;
        }

        deadline = sent_time.QuadPart + SYSTEM_TIME_RESOLUTION; // add 1s

        while (true) {
            LARGE_INTEGER now;
            pollfd pfd;

            status = NtQuerySystemTime(&now);
            if (!success(status)) {
                print_stderr("NtQuerySystemTime returned {}\n", status);
                return;
            }

            if (now.QuadPart >= deadline)
                break;

            pfd.fd = (int)sock;
            pfd.events = POLLIN;

            ret = poll(&pfd, 1, (int)((deadline - now.QuadPart) / 10000)); // convert units of 100ns to ms

            if (ret == 0)
                break;
            else if (ret < 0) {
                print_stderr("poll failed (errno {})\n", errno_to_string_view(*_errno()));
                return;
            }

            auto addrlen = (int)sizeof(addr);
            auto len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&addr,
                                &addrlen);
            if (len < 0) {
                print_stderr("recv failed (errno {})\n", errno_to_string_view(*_errno()));
                continue;
            }

            auto sp = span(buf, len);

            if (sp.size() < sizeof(ipv4_header))
                continue;

            auto& h = *(ipv4_header*)sp.data();

            if (sp.size() < h.ihl * sizeof(uint32_t))
                continue;

            sp = sp.subspan(h.ihl * sizeof(uint32_t));

            if (sp.size() < sizeof(icmpv4_header))
                continue;

            switch (((icmpv4_header*)sp.data())->type) {
                using enum icmpv4_type;

                case dest_unreachable:
                case time_exceeded:
                    handle_dest_unreachable_v4(sp, addr, echo.identifier);
                break;

                case echo_reply:
                    handle_echo_reply_v4(sp, sent_time.QuadPart, addr,
                                         echo.identifier);
                break;

                default:
                    break;
            }
        }

        echo.sequence = byteswap((uint16_t)(byteswap(echo.sequence) + 1));
    }

    // FIXME - print summary before quitting
}

int main(int argc, char** argv) {
    bool ipv4 = false, ipv6 = false;

    static const option options[] = {
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "46", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case '4':
                ipv4 = true;
                break;

            case '6':
                ipv6 = true;
                break;

            case '?':
                return 1;
        }
    }

    if (ipv4 && ipv6) {
        print_stderr("both -4 and -6 specified\n");
        return 1;
    }

    if (optind == argc) {
        print_stderr("ping: usage error: Destination address required\n");
        return 1;
    }

    if (argc > optind + 1) {
        // FIXME - Linux prints usage in this case
        print_stderr("ping: too many cmdline options\n");
        return 1;
    }

    const char* dest = argv[optind];

    addrinfo* ai;
    addrinfo hints;

    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

    if (ipv4)
        hints.ai_family = AF_INET;
    else if (ipv6)
        hints.ai_family = AF_INET6;
    else
        hints.ai_family = AF_UNSPEC;

    if (auto res = getaddrinfo(dest, nullptr, &hints, &ai); res != 0) {
        print_stderr("ping: {}: {}\n", dest, gai_strerror(res));
        return 1;
    }

    bool done = false;
    for (addrinfo* rp = ai; rp; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET6 && rp->ai_family != AF_INET)
            continue;

        if (rp->ai_family == AF_INET6) {
            auto sock = socket(rp->ai_family, SOCK_RAW, IPPROTO_ICMPV6);
            if (sock < 0) {
                print_stderr("socket failed (errno {})\n", errno_to_string_view(*_errno()));
                freeaddrinfo(ai);
                return 1;
            }

            do_ping_v6(sock, *rp, dest);

            closesocket(sock);
        } else if (rp->ai_family == AF_INET) {
            auto sock = socket(rp->ai_family, SOCK_RAW, IPPROTO_ICMP);
            if (sock < 0) {
                print_stderr("socket failed (errno {})\n", errno_to_string_view(*_errno()));
                freeaddrinfo(ai);
                return 1;
            }

            do_ping_v4(sock, *rp, dest);

            closesocket(sock);
        }

        done = true;
        break;
    }

    if (!done)
        print_stderr("ping: {}: no usable address returned\n", dest);

    freeaddrinfo(ai);

    return done ? 0 : 1;
}
