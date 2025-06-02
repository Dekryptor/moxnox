module;

#include "kernel.h"
import objects;
import event;
#include "thread.h"
#include "spinlock.h"

export module pipe;

import print;
import mem;
import config;
import list;
import bsod;
import chardev;

using namespace std;

export class pipe : public chardev {
public:
    pipe(unsigned int buffer_pages);
    virtual ~pipe();

    NTSTATUS read(std::span<uint8_t> s, uint64_t& read_bytes) override;

    NTSTATUS write(std::span<const uint8_t> s, uint64_t& written_bytes) override;

    NTSTATUS write(std::string_view sv, uint64_t& written_bytes) {
        return write(std::span((uint8_t*)sv.data(), sv.length()), written_bytes);
    }

    void clear() override;

    uint8_t* buf;
    unsigned int buf_pages;
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;
    spinlock lock;
    event not_full_event;
    list_entry flushable_le;
};

static spinlock flushable_pipes_lock;
static list_entry flushable_pipes = { &flushable_pipes, &flushable_pipes };

pipe::pipe(unsigned int buffer_pages) : buf_pages(buffer_pages) {
    signalled = false;
    not_full_event.autoreset = false;
    not_full_event.signalled = true;

    if (buffer_pages == 0) {
        buf = nullptr;
        return;
    }

    auto va = allocate_va(buffer_pages);
    if (!va) {
        // FIXME - exception
        print("allocate_va failed\n");
        return;
    }

    for (unsigned int i = 0; i < buffer_pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            // FIXME - exception
            print("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va + (j << PAGE_SHIFT));

                free_folio(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, buffer_pages);
            return;
        }

        if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
            // FIXME - exception
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va + (j << PAGE_SHIFT));

                free_folio(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_folio(pa);
            free_va(va, buffer_pages);
            return;
        }
    }

    buf = (uint8_t*)va;
}

pipe::~pipe() {
    if (!buf)
        return;

    auto va = (uintptr_t)buf;

    for (unsigned int i = 0; i < buf_pages; i++) {
        auto pa = va_to_pa_lookup(va);

        free_folio(pa);
        unmap_page(va);

        va += PAGE_SIZE;
    }

    free_va((uintptr_t)buf, buf_pages);
}

nt_status pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    auto orig_s = s;

    if (readonly) {
        written_bytes = 0;
        return nt_status::MEDIA_WRITE_PROTECTED;
    }

    if (s.empty()) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    do {
        size_t to_copy;

        {
            lock_guard l(lock);

            size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;

            to_copy = min(s.size(), buf_left);

            if (to_copy > 0) {
                if (buf_write_pos + to_copy > buf_pages << PAGE_SHIFT) {
                    auto first_bit = (buf_pages << PAGE_SHIFT) - buf_write_pos;

                    memcpy(&buf[buf_write_pos], s.data(), first_bit);

                    buf_write_pos = 0;
                    buf_unread += first_bit;
                    to_copy -= first_bit;
                    s = s.subspan(first_bit);
                }

                memcpy(&buf[buf_write_pos], s.data(), to_copy);
                buf_write_pos += to_copy;
                buf_unread += to_copy;
                s = s.subspan(to_copy);
            }
        }

        if (to_copy > 0) {
            // wake up waiting thread

            sync_lock sl;
            lock_guard l(sl);

            if (!signalled) {
                signalled = true;

                wake_up_waiters(waiters);
            }

            {
                lock_guard l2(lock);

                if (buf_unread == buf_pages << PAGE_SHIFT)
                    not_full_event.signalled = false;
            }
        }

        if (s.empty())
            break;

        // block while we wait for space to become available

        if (wait_for_object(not_full_event, true) == nt_status::ALERTED)
            break;
    } while (true);

    written_bytes = (uint64_t)(s.data() - orig_s.data());

    return nt_status::SUCCESS;
}

NTSTATUS pipe::read(span<uint8_t> s, uint64_t& read_bytes) {
    size_t copied = 0;

    if (s.empty()) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    acquire();
    object_ref obj{*this}; // so refcount gets decreased on exit

    auto status = wait_for_object(*this, true);

    if (status == nt_status::ALERTED) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    } else if (!success(status))
        return status;

    {
        sync_lock sl;
        lock_guard l(sl);
        lock_guard l2(lock);

        if (buf_unread == 0) {
            read_bytes = 0;
            return nt_status::SUCCESS;
        }

        size_t buf_read_pos = buf_write_pos + (buf_pages << PAGE_SHIFT) - buf_unread;

        while (buf_read_pos >= buf_pages << PAGE_SHIFT) {
            buf_read_pos -= buf_pages << PAGE_SHIFT;
        }

        size_t to_copy = min(s.size(), buf_unread);

        if (buf_read_pos + to_copy > buf_pages << PAGE_SHIFT) {
            auto first_bit = (buf_pages << PAGE_SHIFT) - buf_read_pos;

            memcpy(s.data(), &buf[buf_read_pos], first_bit);

            buf_read_pos = 0;
            buf_unread -= first_bit;
            to_copy -= first_bit;
            s = s.subspan(first_bit);
            copied = first_bit;
        }

        memcpy(s.data(), &buf[buf_read_pos], to_copy);
        buf_unread -= to_copy;
        copied += to_copy;

        if (buf_unread == 0)
            signalled = false;

        if (!not_full_event.signalled)
            not_full_event.set_holding_sync_lock();
    }

    read_bytes = copied;
    return nt_status::SUCCESS;
}

void pipe::clear() {
    sync_lock sl;
    lock_guard l(sl);
    lock_guard l2(lock);

    buf_write_pos = 0;
    buf_unread = 0;
    signalled = false;

    if (not_full_event.signalled) {
        not_full_event.signalled = false;
        wake_up_waiters(not_full_event.waiters);
    }
}

export void add_to_flushable_pipes_list(pipe& p) {
    lock_guard lock(flushable_pipes_lock);

    insert_tail_list(flushable_pipes, p.flushable_le);
}

export void shutdown_pipes() {
    while (true) {
        pipe* p;

        {
            lock_guard lock(flushable_pipes_lock);

            if (is_list_empty(flushable_pipes))
                return;

            p = (pipe*)((uint8_t*)flushable_pipes.next - offsetof(pipe, flushable_le));
            remove_entry_list(p->flushable_le);
        }

        p->flush(true);
    }
}
