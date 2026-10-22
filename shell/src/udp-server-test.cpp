// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <bit>

#undef errno

import ucrtbase;
import ws2_32;
import errno;
import print;
import ipv6;

using namespace std;

static const uint16_t PORT = 8080;
static const size_t BUFFER_SIZE = 1500;

int main() {
    SOCKET sockfd = INVALID_SOCKET;
    struct sockaddr_in6 server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        print_stderr("socket failed (errno {})\n", errno_to_string_view(*_errno()));
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = byteswap(PORT);
    // server_addr.sin6_addr = in6addr_any; // FIXME

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        print_stderr("bind failed (errno {})\n", errno_to_string_view(*_errno()));
        closesocket(sockfd);
        return 1;
    }

    print("IPv6 echo server listening on port {}...\n", PORT);

    while (true) {
        int bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr*)&client_addr,
                             &client_addr_len);
        if (bytes < 0) {
            print_stderr("recvfrom failed (errno {})\n", errno_to_string_view(*_errno()));
            continue;
        }

        if (bytes > 0) {
            print("--- {} bytes --- (from {} port {}, netif {})\n", bytes,
                  *(ipv6_address*)&client_addr.sin6_addr, byteswap(client_addr.sin6_port), client_addr.sin6_scope_id);
            for (ssize_t i = 0; i < bytes; i++) {
                if (i % 16 == 0)
                    print("{:04x}: ", i);

                print("{:02x} ", (uint8_t)buffer[i]);

                if (i % 16 == 15 || i == bytes - 1)
                    print("\n");
            }

            auto ret = sendto(sockfd, buffer, bytes, 0,
                              (struct sockaddr*)&client_addr, client_addr_len);
            if (ret < 0)
                print_stderr("sendto failed (errno {})\n", errno_to_string_view(*_errno()));
        }
    }

    closesocket(sockfd);

    return 0;
}
