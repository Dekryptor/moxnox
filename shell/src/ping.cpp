#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <optional>
#include <span>
#include <bit>
#include <string_view>

import nt;
import print;
import ipv6;
import ws2_32;
import ucrtbase;

#undef errno
import errno;

using namespace std;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

static void handle_echo_reply(span<const char> data, int64_t sent_time,
                              const sockaddr_in6& addr, uint16_t identifier) {
    char s[10];
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

    // FIXME - add float support to print
    // FIXME - need higher-quality timer?
    print("{} bytes from {}: icmp_seq={} time={} ms\n", data.size(), *(ipv6_address*)&addr.sin6_addr, byteswap(resp.sequence), string_view(s));
}

static void handle_dest_unreachable(span<const char> data, const sockaddr_in6& addr,
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

static void do_ping(SOCKET sock, const addrinfo& ai) {
    icmpv6_echo echo;
    PROCESS_BASIC_INFORMATION pbi;

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESSINFOCLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        print_stderr("NtQueryInformationProcess returned {}\n", status);
        return;
    }

    auto sockh = (HANDLE)_get_osfhandle(sock);
    if ((intptr_t)sockh == -1) {
        print_stderr("_get_osfhandle failed (errno {})\n", *_errno());
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
            LARGE_INTEGER now, timeout;

            status = NtQuerySystemTime(&now);
            if (!success(status)) {
                print_stderr("NtQuerySystemTime returned {}\n", status);
                return;
            }

            if (now.QuadPart >= deadline)
                break;

            timeout.QuadPart = - (deadline - now.QuadPart);

            // FIXME - should actually be poll
            status = NtWaitForSingleObject(sockh, false, &timeout);

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
                    handle_dest_unreachable(span(buf, (size_t)len), addr,
                                            echo.identifier);
                break;

                case icmpv6_type::echo_reply:
                    handle_echo_reply(span(buf, (size_t)len), sent_time.QuadPart,
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

    addrinfo* ai;

    if (auto res = getaddrinfo(argv[1], nullptr, nullptr, &ai); res != 0) {
        print_stderr("ping: {}: {}\n", argv[1], gai_strerror(res));
        return 1;
    }

    bool done = false;
    for (addrinfo* rp = ai; rp; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET6)
            continue;

        auto sock = socket(rp->ai_family, SOCK_RAW, IPPROTO_ICMPV6);
        if (sock < 0) {
            print_stderr("socket failed (errno {})\n", errno_to_string_view(*_errno()));
            freeaddrinfo(ai);
            return 1;
        }

        do_ping(sock, *rp);

        closesocket(sock);

        done = true;
        break;
    }

    if (!done)
        print_stderr("ping: {}: no usable address returned\n", argv[1]);

    freeaddrinfo(ai);

    return done ? 0 : 1;
}
