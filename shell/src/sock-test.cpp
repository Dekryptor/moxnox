#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>

import ws2_32;
import print;
import ipv4;
import ipv6;

#undef errno
import errno;

template<bool IPV4>
int do_test() {
    int sock;

    if constexpr (IPV4)
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    else
        sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

    if (sock < 0) {
        print("socket failed (errno {})\n", errno_to_string_view(*_errno()));
        return 1;
    }

    print("Listening on all interfaces...\n\n");

    char buf[4096];

    while (1) {
        ssize_t len;

        if constexpr (IPV4) {
            sockaddr_in addr;

            auto addrlen = (int)sizeof(addr);
            len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&addr,
                           &addrlen);
            if (len < 0) {
                print("recv failed (errno {})\n", errno_to_string_view(*_errno()));
                break;
            }

            print("--- {} bytes --- (from {})\n", len, *(ipv4_address*)&addr.sin_addr);
        } else {
            sockaddr_in6 addr;

            auto addrlen = (int)sizeof(addr);
            len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&addr,
                           &addrlen);
            if (len < 0) {
                print("recv failed (errno {})\n", errno_to_string_view(*_errno()));
                break;
            }

            print("--- {} bytes --- (from {}, netif {})\n", len, *(ipv6_address*)&addr.sin6_addr, addr.sin6_scope_id);
        }

        for (ssize_t i = 0; i < len; i++) {
            if (i % 16 == 0)
                print("{:04x}: ", i);

            print("{:02x} ", (uint8_t)buf[i]);

            if (i % 16 == 15 || i == len - 1)
                print("\n");
        }

        print("\n");
    }

    return 0;
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

    if (ipv4)
        return do_test<true>();
    else
        return do_test<false>();
}
