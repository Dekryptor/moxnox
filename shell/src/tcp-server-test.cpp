#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <string_view>
#include <bit>

#undef errno
import errno;
import ws2_32;
import print;
import ipv6;

using namespace std;

int main() {
    struct sockaddr_in6 addr;

    auto server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_sock < 0) {
        print_stderr("socket returned {} (errno {})\n", server_sock, errno_to_string_view(*_errno()));
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = byteswap((uint16_t)8080);

    int ret = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret) {
        print_stderr("bind returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    ret = listen(server_sock, 5);
    if (ret) {
        print_stderr("listen returned {} (errno {})\n", ret,
                     errno_to_string_view(*_errno()));
        closesocket(server_sock);
        return 1;
    }

    print("Echo server listening on port 8080...\n");

    while (true) {
        struct sockaddr_in6 client_addr;
        int client_addrlen = sizeof(client_addr);
        char buf[4096];

        auto client_sock = accept(server_sock, (struct sockaddr*)&client_addr,
                                  &client_addrlen);
        if (client_sock < 0) {
            print_stderr("accept returned {} (errno {})\n", client_sock,
                         errno_to_string_view(*_errno()));
            closesocket(server_sock);
            return 1;
        }

        if (client_addrlen != sizeof(client_addr)) {
            print_stderr("client_addrlen was {}, expected {}\n", client_addrlen,
                         sizeof(client_addr));
            closesocket(client_sock);
            closesocket(server_sock);
            return 1;
        }

        print("Connected from [{}]:{}, socket {}\n", *(ipv6_address*)client_addr.sin6_addr,
              byteswap(client_addr.sin6_port), (uintptr_t)client_sock);

        auto bytes = recv(client_sock, buf, sizeof(buf), 0);
        if (bytes < 0) {
            print_stderr("recv returned {} (errno {})\n", bytes,
                         errno_to_string_view(*_errno()));
            closesocket(client_sock);
            closesocket(server_sock);
            return 1;
        }

        print("Received: \"{}\"\n", string_view(buf, (size_t)bytes));

        closesocket(client_sock);
    }

    closesocket(server_sock);

    return 0;
}
