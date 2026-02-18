#include <errno.h>
#include <stdint.h>

import ws2_32;
import print;
import ipv6;

static const ipv6_address IPV6_ALL_ROUTERS = {0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};

static bool send_router_solicitation(SOCKET sock) {
    icmpv6_router_solicitation rs;
    sockaddr_in6 dst;

    rs.type = icmpv6_type::router_solicitation;
    rs.code = 0;
    rs.checksum = 0;
    rs.reserved = 0;

    // FIXME - should include source LL address as option

    // FIXME - calculate checksum

    dst.sin6_family = AF_INET6;
    dst.sin6_port = 0;
    dst.sin6_flowinfo = 0;
    *(ipv6_address*)&dst.sin6_addr = IPV6_ALL_ROUTERS;
    dst.sin6_scope_id = 0; // FIXME

    auto ret = sendto(sock, (char*)&rs, sizeof(rs), 0, (sockaddr*)&dst,
                      sizeof(dst));
    if (ret < 0) {
        print("sendto failed (errno {})\n", errno);
        return false;
    }

    return true;
}

static bool find_routers() {
    // FIXME - specs say we're supposed to wait between 0 and 1 second

    auto sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        print_stderr("socket failed (errno {})\n", errno);
        return false;
    }

    // FIXME - find link-local address and bind to it

    // FIXME - run once for each interface

    if (!send_router_solicitation(sock))
        return false;

    // FIXME - wait 4 seconds for response
    // FIXME - if no RA, send RS 2
    // FIXME - wait 4 seconds for response
    // FIXME - if no RA, send RS 3
    // FIXME - wait 4 seconds for response
    // FIXME - give up

    return true;
}

int main() {
    if (!find_routers())
        return 1;

    return 0;
}
