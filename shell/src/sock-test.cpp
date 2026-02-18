#include <errno.h>
#include <stdint.h>

import ws2_32;
import print;
import ipv6;

int main() {
    auto sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        print("socket failed (errno {})\n", errno);
        return 1;
    }

    print("Listening on all interfaces...\n\n");

    char buf[4096];
    sockaddr_in6 addr;

    while (1) {
        auto addrlen = (int)sizeof(addr) + 4;
        ssize_t len = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&addr,
                               &addrlen);
        if (len < 0) {
            print("recv failed (errno {})\n", errno);
            break;
        }

        print("--- {} bytes --- (from {})\n", len, *(ipv6_address*)&addr.sin6_addr);
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
