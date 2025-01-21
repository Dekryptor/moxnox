#include "kernel.h"
#include "list.h"
#include "objects.h"
#include "handle.h"

using namespace std;

object_ref create_pipe(unsigned int buffer_pages) {
    auto pptr = new pipe(buffer_pages);
    if (!pptr) {
        printk("new failed\n");
        return {};
    }

    auto& p = *pptr;

    return p;
}

pipe::pipe(unsigned int buffer_pages) : object(object_type::pipe), buf_pages(buffer_pages) {
    not_full_event.autoreset = false;
    not_full_event.signalled = true;

    if (buffer_pages == 0) {
        buf = nullptr;
        return;
    }

    auto va = allocate_va(buffer_pages);
    if (!va) {
        // FIXME - exception
        printk("allocate_va failed\n");
        return;
    }

    for (unsigned int i = 0; i < buffer_pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            // FIXME - exception
            printk("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa(va + (j << PAGE_SHIFT));

                free_page(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, buffer_pages);
            return;
        }

        if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
            // FIXME - exception
            printk("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa(va + (j << PAGE_SHIFT));

                free_page(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_page(pa);
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
        auto pa = va_to_pa(va);

        free_page(pa);
        unmap_page(va);

        va += PAGE_SIZE;
    }

    free_va((uintptr_t)buf, buf_pages);
}

size_t pipe::write(span<const uint8_t> s) {
    auto size = s.size();

    if (s.empty())
        return 0;

    do {
        unsigned int release_count = 0;
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

                {
                    auto le = thread_list.next;

                    while (le != &thread_list) {
                        auto& thr = containing_record<thread>(le);

                        if (thr.status == thread_status::waiting && thr.waiting_for.has_value() && &thr.waiting_for->get() == static_cast<object*>(this)) {
                            thr.status = thread_status::runnable;
                            thr.waiting_for = nullopt;
                            release_count++;
                        }

                        le = le->next;
                    }
                }
            }

            {
                lock_guard l2(lock);

                if (buf_unread == buf_pages << PAGE_SHIFT)
                    not_full_event.signalled = false;
            }
        }

        for (unsigned int i = 0; i < release_count; i++) {
            release();
        }

        if (s.empty())
            break;

        // block while we wait for space to become available

        wait_for_object(not_full_event, false);
    } while (true);

    return size;
}

NTSTATUS pipe::read(span<uint8_t> s, uint64_t& read_bytes) {
    size_t copied = 0;

    if (s.empty()) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    acquire();
    wait_for_object(*this, false);

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
