#include <stdint.h>
#include <stdlib.h>
#include <bit>

#undef errno
import print;
import ws2_32;
import ipv6;
import errno;

using namespace std;

static void test(sockaddr_in6& s) {
    // Create socket
    int sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    print("socket() = {}\n", sock);

    if (sock < 0) {
        print_stderr("socket returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return;
    }

    struct sockaddr_in6 addr = {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = byteswap((uint16_t)80);
    *(ipv6_address*)addr.sin6_addr = *(ipv6_address*)s.sin6_addr;

    print("Connecting...\n");
    int ret = connect(sock, (sockaddr*)&addr, sizeof(addr));

    if (ret == 0)
        print("SUCCESS! TCP connection works!\n");
    else {
        print_stderr("connect returned {} (errno {})\n", ret, errno_to_string_view(*_errno()));
        closesocket(sock);
        return;
    }

    closesocket(sock);
}

int main() {
    print("Simple TCP connection test\n");

    addrinfo* ai;

    if (auto res = getaddrinfo("moxnox.dev", nullptr, nullptr, &ai); res != 0) {
        print_stderr("getaddrinfo failed: {}\n", gai_strerror(res));
        return 1;
    }

    bool done = false;
    for (addrinfo* rp = ai; rp; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET6)
            continue;

        auto& s = *((sockaddr_in6*)ai->ai_addr);

        print("found IP address {}\n", *(ipv6_address*)s.sin6_addr);

        test(s);
        done = true;
        break;
    }

    if (!done) {
        print_stderr("no suitable address found\n");
        freeaddrinfo(ai);
        return 1;
    }

    freeaddrinfo(ai);

    return 0;
}
