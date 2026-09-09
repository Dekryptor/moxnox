module;

#include <span>
#include <mutex>
#include <algorithm>
#include <stdint.h>

module net;

import print;
import config;
import vec;
import scheduler;
import event;

using namespace std;

class unix_socket : public socket {
public:
    unix_socket();

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

    spinlock lock;
    unix_socket* other_end = nullptr;
    bool dead = false;
    bool first;
    event not_full_event;

    uint8_t buf[PAGE_SIZE];
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;

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
nt_status unix_socket::do_sendto(span<const span<const uint8_t>> v, int flags,
                                 sockaddr*, size_t, uint64_t& sent_bytes) {
    if (!other_end)
        return nt_status::INVALID_PARAMETER;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        sent_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<const uint8_t> top = v.front();

    sent_bytes = 0;

    do {
        size_t to_copy;

        {
            lock_guard l(other_end->lock);

            size_t buf_left = sizeof(other_end->buf) - other_end->buf_unread;

            if (other_end->dead) {
                if (sent_bytes == 0)
                    return nt_status::PIPE_DISCONNECTED;
                else
                    break;
            }

            to_copy = min(vec_size, buf_left);

            if (to_copy > 0) {
                if (other_end->buf_write_pos + to_copy > sizeof(other_end->buf)) {
                    auto first_bit = sizeof(other_end->buf) - other_end->buf_write_pos;

                    memcpy_vec(&other_end->buf[other_end->buf_write_pos], v, top,
                               first_bit);

                    other_end->buf_write_pos = 0;
                    other_end->buf_unread += first_bit;
                    to_copy -= first_bit;
                    vec_size -= first_bit;
                    sent_bytes += first_bit;
                }

                memcpy_vec(&other_end->buf[other_end->buf_write_pos], v, top,
                           to_copy);

                other_end->buf_write_pos += to_copy;
                other_end->buf_unread += to_copy;
                vec_size -= to_copy;
                sent_bytes += to_copy;
            }
        }

        if (to_copy > 0) {
            // wake up waiting thread

            sync_lock sl;
            lock_guard l(sl);

            if (!other_end->signalled) {
                other_end->signalled = true;

                wake_up_waiters(other_end->waiters, WAIT_TYPE_READING);
            }

            {
                lock_guard l2(other_end->lock);

                if (other_end->buf_unread == sizeof(other_end->buf) && !other_end->dead) {
                    other_end->not_full_event.signalled = false;
                    write_signalled = false;
                }
            }
        }

        if (vec_size == 0)
            break;

        if constexpr (NONBLOCK) {
            if (sent_bytes == 0)
                return nt_status::CANT_WAIT;
            else
                break;
        } else {
            // block while we wait for space to become available
            auto status = wait_for_object(other_end->not_full_event, true);

            if (status == nt_status::USER_APC) {
                if (sent_bytes == 0)
                    return nt_status::USER_APC;
                else
                    break;
            } else if (!success(status))
                return status;
        }
    } while (true);

    return nt_status::SUCCESS;
}

unix_socket::unix_socket() {
    signalled = false;
    not_full_event.autoreset = false;
    not_full_event.signalled = true;
}

void unix_socket::last_handle_closed() {
    if (other_end) {
        bool both_dead;

        {
            auto& first_lock = first ? lock : other_end->lock;
            auto& second_lock = first ? other_end->lock : lock;

            lock_guard lg1(first_lock);
            lock_guard lg2(second_lock);

            dead = true;
            both_dead = other_end->dead;
        }

        if (both_dead) {
            other_end->release();
            release();
        }
    }
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

nt_status create_unix_socket_pair(int type, object_ref& ref1, object_ref& ref2) {
    if (type != SOCK_STREAM) // FIXME - support SOCK_DGRAM too
        return nt_status::INVALID_PARAMETER;

    auto s1 = new unix_socket;
    if (!s1) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref1 = object_ref{*s1};

    auto s2 = new unix_socket;
    if (!s2) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref2 = object_ref{*s2};

    s1->other_end = s2;
    s1->first = true;

    s2->other_end = s1;
    s2->first = false;

    s1->acquire();
    s2->acquire();

    return nt_status::SUCCESS;
}
