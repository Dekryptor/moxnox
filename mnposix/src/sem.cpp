// SPDX-License-Identifier: LGPL-2.1-or-later
#include <semaphore.h>
#include <atomic>
#include <limits>

import nt;
import print;
import ucrtbase;

#undef errno
import errno;

using namespace std;

static uint32_t extract_count(uint64_t value) {
    return (uint32_t)(value >> 32);
}

static uint32_t extract_waiters(uint64_t value) {
    return (uint32_t)value;
}

static uint64_t create_value(uint32_t count, uint32_t waiters) {
    return (uint64_t)count << 32 | waiters;
}

extern "C"
int sem_init(sem_t* sem, int pshared, unsigned int count) {
    atomic_ref<uint64_t> r(sem->_x[0]);

    if (pshared) {
        print_stderr("FIXME - sem_init with pshared\n");
        // FIXME - would need named kernel objects?
        *_errno() = ENOTSUP;
        return -1;
    }

    r.store(create_value(count, 0));
    sem->_x[1] = 0;

    return 0;
}

extern "C"
int sem_destroy(sem_t* sem) {
    atomic_ref<HANDLE> ev(sem->_x[1]);
    auto h = ev.load();

    if (h)
        NtClose(h);

    return 0;
}

extern "C"
int sem_wait(sem_t* sem) {
    atomic_ref<uint64_t> r(sem->_x[0]);
    atomic_ref<HANDLE> ev(sem->_x[1]);
    HANDLE event_handle;
    nt_status status;

    while (true) {
        uint64_t v = r.load(memory_order_relaxed);

        if (extract_count(v) > 0) {
            uint64_t newval = create_value(extract_count(v) - 1,
                                           extract_waiters(v));

            if (!r.compare_exchange_weak(v, newval, memory_order_acquire,
                                         memory_order_relaxed)) {
                continue;
            }

            if (extract_count(v) > 1 && extract_waiters(v) > 0)
                NtSetEvent(ev.load(), nullptr);

            return 0;
        }

        event_handle = ev.load(memory_order_relaxed);

        if (event_handle == 0) {
            HANDLE h;

            status = NtCreateEvent(&h, 0, nullptr, EVENT_TYPE::SynchronizationEvent,
                                   false);
            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);
                return -1;
            }

            if (ev.compare_exchange_strong(event_handle, h, memory_order_acquire,
                                           memory_order_relaxed)) {
                event_handle = h;
            } else
                NtClose(h);
        }

        uint64_t newval = create_value(0, extract_waiters(v) + 1);

        if (!r.compare_exchange_weak(v, newval, memory_order_acquire,
                                     memory_order_relaxed)) {
            continue;
        }

        while (true) {
            auto sig_count = mn_handled_nonrestartable_signal_count();

            status = NtWaitForSingleObject(event_handle, false, nullptr);

            if (status == nt_status::USER_APC) {
                if (mn_handled_nonrestartable_signal_count() != sig_count) {
                    *_errno() = EINTR;

                    while (true) {
                        v = r.load(memory_order_relaxed);
                        newval = create_value(extract_count(v), extract_waiters(v) - 1);

                        if (r.compare_exchange_weak(v, newval, memory_order_acquire,
                                                    memory_order_relaxed)) {
                            break;
                        }
                    }

                    return -1;
                }

                continue;
            }

            if (!success(status)) {
                *_errno() = nt_status_to_errno(status);

                while (true) {
                    v = r.load(memory_order_relaxed);
                    newval = create_value(extract_count(v), extract_waiters(v) - 1);

                    if (r.compare_exchange_weak(v, newval, memory_order_acquire,
                                                memory_order_relaxed)) {
                        break;
                    }
                }

                return -1;
            }

            break;
        }

        while (true) {
            v = r.load(memory_order_relaxed);
            newval = create_value(extract_count(v), extract_waiters(v) - 1);

            if (r.compare_exchange_weak(v, newval, memory_order_acquire,
                                        memory_order_relaxed)) {
                break;
            }
        }
    }
}

extern "C"
int sem_trywait(sem_t* sem) {
    atomic_ref<uint64_t> r(sem->_x[0]);
    atomic_ref<HANDLE> ev(sem->_x[1]);

    while (true) {
        uint64_t v = r.load(memory_order_relaxed);

        if (extract_count(v) > 0) {
            uint64_t newval = create_value(extract_count(v) - 1,
                                           extract_waiters(v));

            if (!r.compare_exchange_weak(v, newval, memory_order_acquire,
                                         memory_order_relaxed)) {
                continue;
            }

            if (extract_count(v) > 1 && extract_waiters(v) > 0)
                NtSetEvent(ev.load(), nullptr);

            return 0;
        }

        break;
    }

    *_errno() = EAGAIN;
    return -1;
}

extern "C"
int sem_post(sem_t* sem) {
    atomic_ref<uint64_t> r(sem->_x[0]);
    atomic_ref<HANDLE> ev(sem->_x[1]);

    auto v = r.fetch_add(create_value(1, 0), memory_order_release);

    if (extract_waiters(v) > 0)
        NtSetEvent(ev.load(), nullptr);

    return 0;
}
