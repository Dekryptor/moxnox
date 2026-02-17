#include <errno.h>
#include <stdint.h>

import ws2_32;
import print;

int main() {
    auto sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        print("socket failed (errno {})\n", errno);
        return 1;
    }

    print("Listening on all interfaces...\n\n");

    char buf[4096];

    while (1) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len < 0) {
            print("recv failed (errno {})\n", errno);
            break;
        }

        print("--- {} bytes ---\n", len);
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
