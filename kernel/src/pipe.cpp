#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"

using namespace std;

unique_handle create_pipe(unsigned int buffer_pages) {
    auto pptr = new pipe(buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    auto hv = add_kernel_handle(p, 0); // FIXME - access mask

    p.release(); // add_kernel_handle increases refcount

    return unique_handle{hv};
}

pipe::pipe(unsigned int buffer_pages) : object(object_type::pipe), buf_pages(buffer_pages) {
    if (buffer_pages == 0) {
        // FIXME - exception
        print("Invalid value for buffer_pages\n");
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
                auto pa = va_to_pa(va + (j << PAGE_SHIFT));

                free_page(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, buffer_pages);
            return;
        }

        if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
            // FIXME - exception
            print("map_page failed\n");

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
    unsigned int release_count = 0;
    auto size = s.size();

    if (s.empty())
        return 0;

    {
        lock_guard l(lock);

        size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;
        size_t to_copy = min(s.size(), buf_left);

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

        // wake up waiting thread

        {
            sync_lock sl;
            lock_guard lock(sl);

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
        }
    }

    for (unsigned int i = 0; i < release_count; i++) {
        release();
    }

    if (s.empty())
        return size;

    // FIXME - block while we wait for space to become available

    return size;
}

size_t pipe::read(span<uint8_t> s) {
    size_t copied = 0;

    if (s.empty())
        return 0;

    acquire();
    wait_for_object(*this);

    {
        lock_guard l(lock);

        if (buf_unread == 0)
            return 0;

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

        if (buf_unread == 0) {
            sync_lock sl;
            lock_guard l2(sl);
            signalled = false;
        }
    }

    return copied;
}
