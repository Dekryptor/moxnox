module;

#include <span>
#include <stdint.h>

module net;

import print;

using namespace std;

class unix_socket : public socket {
public:
    void last_handle_closed() override;

    nt_status recvfrom(span<const span<uint8_t>> v, int flags,
                       sockaddr_storage* from, size_t* fromlen,
                       uint64_t& recv_bytes) override {
        return do_recvfrom<false>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto(span<const span<const uint8_t>> v, int flags, sockaddr* to,
                     size_t tolen, uint64_t& sent_bytes) override {
        return do_sendto<false>(v, flags, to, tolen, sent_bytes);
    }

    nt_status bind(const sockaddr* addr, size_t addrlen) override;
    nt_status connect(const sockaddr* addr, size_t addrlen) override;
    nt_status listen(int backlog) override;
    nt_status accept(HANDLE& sock, void* addr, int& addrlen) override;
    nt_status getsockname(span<uint8_t>, size_t&) override;
    nt_status getpeername(span<uint8_t> buf, size_t& len) override;

    nt_status recvfrom_nonblock(span<const span<uint8_t>> v, int flags,
                                sockaddr_storage* from, size_t* fromlen,
                                uint64_t& recv_bytes) override {
        return do_recvfrom<true>(v, flags, from, fromlen, recv_bytes);
    }

    nt_status sendto_nonblock(span<const span<const uint8_t>> v, int flags,
                              sockaddr* to, size_t tolen,
                              uint64_t& sent_bytes) override {
        return do_sendto<true>(v, flags, to, tolen, sent_bytes);
    }

    nt_status shutdown(int how) override;

private:
    template<bool NONBLOCK>
    nt_status do_recvfrom(span<const span<uint8_t>> v, int flags,
                          sockaddr_storage* from, size_t* fromlen,
                          uint64_t& recv_bytes);

    template<bool NONBLOCK>
    nt_status do_sendto(span<const span<const uint8_t>> v, int flags,
                        sockaddr* to, size_t tolen, uint64_t& sent_bytes);
};

nt_status create_new_unix_socket(object_ref& ref) {
    auto s = new unix_socket;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

template<bool NONBLOCK>
nt_status unix_socket::do_recvfrom(span<const span<uint8_t>> /*v*/, int /*flags*/,
                                   sockaddr_storage* /*from*/, size_t* /*fromlen*/,
                                   uint64_t& /*recv_bytes*/) {
    print("FIXME - unix_socket::do_recvfrom\n");

    return nt_status::NOT_IMPLEMENTED;
}

template<bool NONBLOCK>
nt_status unix_socket::do_sendto(span<const span<const uint8_t>> /*v*/, int /*flags*/,
                                 sockaddr* /*to*/, size_t /*tolen*/, uint64_t& /*sent_bytes*/) {
    print("FIXME - unix_socket::do_sendto\n");

    return nt_status::NOT_IMPLEMENTED;
}

void unix_socket::last_handle_closed() {
    print("FIXME - unix_socket::last_handle_closed\n");
}

nt_status unix_socket::bind(const sockaddr* /*addr*/, size_t /*addrlen*/) {
    print("FIXME - unix_socket::bind\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::connect(const sockaddr* /*addr*/, size_t /*addrlen*/) {
    print("FIXME - unix_socket::connect\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::listen(int /*backlog*/) {
    print("FIXME - unix_socket::listen\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::accept(HANDLE& /*sock*/, void* /*addr*/, int& /*addrlen*/) {
    print("FIXME - unix_socket::accept\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::getsockname(span<uint8_t>, size_t&) {
    print("FIXME - unix_socket::getsockname\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::getpeername(span<uint8_t> /*buf*/, size_t& /*len*/) {
    print("FIXME - unix_socket::getpeername\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::shutdown(int /*how*/) {
    print("FIXME - unix_socket::shutdown\n");

    return nt_status::NOT_IMPLEMENTED;
}
