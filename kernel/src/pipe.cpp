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

    auto hv = add_handle(p, 0); // FIXME - access mask

    p.release(); // add_handle increases refcount

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

void pipe::write(span<const uint8_t> s) {
    if (s.empty())
        return;

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
            s = s.subspan(to_copy);
        }

        memcpy(&buf[buf_write_pos], s.data(), to_copy);
        buf_write_pos += to_copy;
        buf_unread += to_copy;
        s = s.subspan(to_copy);
    }

    // FIXME - wake up single waiting thread
    // FIXME - otherwise, mark as signalled (make sure wait_for_object clears this)

    if (s.empty())
        return;

    // FIXME - block while we wait for space to become available
}
