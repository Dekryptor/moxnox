module;

#include <span>
#include <mutex>
#include <algorithm>
#include <stdint.h>

module framebuffer;

import nt;
import print;
import mem;
import config;
import handle;
import section;
import page_cache;
import moxnox_ioctls;
import crt;
import thread;
import spinlock;
import disable_interrupts;

using namespace std;

static spinlock cursor_lock;
static uint32_t* cursor_buffer;
static uint16_t cursor_width, cursor_height, cursor_hotspot_x, cursor_hotspot_y;
static uint16_t cursor_x, cursor_y;

template<bool BOOT>
class framebuffer_cache : public custom_section_cache {
public:
    NTSTATUS read_into_cache(uint64_t page_num, uint64_t num_pages,
                             uint64_t& read_pages) override;
    NTSTATUS flush_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                        span<uint8_t> buf, size_t* return_length) override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;

private:
    nt_status ioctl_set_cursor(span<const uint8_t> inbuf);
};

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                                   uint64_t& read_pages) {
    auto status = cache.pin(page_num, num_pages, nullptr);
    if (!success(status)) {
        print("page_cache::pin returned {}\n", status);
        return status;
    }

    span<uint32_t> fb;

    if constexpr (BOOT)
        fb = shadow_framebuffer;
    else
        fb = framebuffer;

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(page_num);
            else
                p = cache.next_page_cache_entry(*p);
        }

        if (p->pfn)
            continue;

        auto va = (uintptr_t)((uint8_t*)fb.data() + ((page_num + i) << PAGE_SHIFT));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = va_to_pa_lookup(va) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    cache.unpin(page_num, num_pages);

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

template<bool BOOT>
NTSTATUS framebuffer_cache<BOOT>::flush_cache(uint64_t page_num, uint64_t num_pages) {
    auto start = page_num << PAGE_SHIFT;
    span<uint32_t> fb;

    if constexpr (BOOT)
        fb = shadow_framebuffer;
    else
        fb = framebuffer;

    if (start >= fb.size_bytes())
        return nt_status::SUCCESS;

    if constexpr (BOOT) {
        lock_guard lock(cursor_lock);

        auto min_y = (uint16_t)((page_num << PAGE_SHIFT) / (fb_pixels_per_scan_line * sizeof(uint32_t)));
        auto max_y = min((uint16_t)(((page_num + num_pages + 1) << PAGE_SHIFT) / (fb_pixels_per_scan_line * sizeof(uint32_t))), (uint16_t)(fb_height - 1));
        auto cursor_top = (int16_t)((int16_t)cursor_y - (int16_t)cursor_hotspot_y);
        auto cursor_bottom = (int16_t)(cursor_top + cursor_height);
        auto cursor_left = (int16_t)((int16_t)cursor_x - (int16_t)cursor_hotspot_x);
        auto draw_x_start = (uint32_t)max((int16_t)0, cursor_left);
        auto draw_x_end = (uint32_t)min((int16_t)fb_width, (int16_t)(cursor_left + cursor_width));

        for (uint16_t y = min_y; y <= max_y; y++) {
            auto fb_line = &framebuffer[y * fb_pixels_per_scan_line];
            auto shadow_line = &shadow_framebuffer[y * fb_pixels_per_scan_line];

            if (y >= cursor_top && y < cursor_bottom) {
                auto cursor_line = &cursor_buffer[((int16_t)y - cursor_top) * cursor_width];

                memcpy(fb_line, shadow_line, draw_x_start * sizeof(uint32_t));
                memcpy(&fb_line[draw_x_start], &cursor_line[(int16_t)draw_x_start - cursor_left],
                       (draw_x_end - draw_x_start) * sizeof(uint32_t));
                memcpy(&fb_line[draw_x_end], &shadow_line[draw_x_end],
                       (fb_width - draw_x_end) * sizeof(uint32_t));
            } else
                memcpy(fb_line, shadow_line, fb_width * sizeof(uint32_t));
        }
    } else {
        auto y1 = (unsigned int)(((page_num << PAGE_SHIFT) / sizeof(uint32_t)) / fb_width);
        auto y2 = (unsigned int)((((page_num + num_pages) << PAGE_SHIFT) / sizeof(uint32_t)) / fb_width);

        flush_framebuffer(0, y1, fb_width, min(fb_height - y1, y2 - y1 + 1));
    }

    return nt_status::SUCCESS;
}

template<bool BOOT>
NTSTATUS framebuffer_cache<BOOT>::query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                                             span<uint8_t> buf, size_t* return_length) {
    switch (section_information_class) {
        case SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation: {
            if (buf.size() < sizeof(MN_SECTION_FRAMEBUFFER_INFORMATION))
                return nt_status::BUFFER_TOO_SMALL;

            auto& info = *(MN_SECTION_FRAMEBUFFER_INFORMATION*)buf.data();

            info.width = fb_width;
            info.height = fb_height;
            info.pixels_per_scan_line = fb_pixels_per_scan_line;
            info.dpi = fb_dpi;

            *return_length = sizeof(MN_SECTION_FRAMEBUFFER_INFORMATION);

            return nt_status::SUCCESS;
        }

        default:
            return nt_status::INVALID_INFO_CLASS;
    }
}

void create_boot_framebuffer_section() {
    nt_status status;

    auto s = new section;
    if (!s) {
        print("new failed\n");
        return;
    }

    s->type = section_type::anonymous;
    s->size = (uint32_t)shadow_framebuffer.size_bytes();
    s->num_pages = bytes_to_pages(s->size);
    s->page_protection = PAGE_READWRITE;

    s->custom_cache = new framebuffer_cache<true>;
    if (!s->custom_cache) {
        print("new failed\n");
        s->release();
        return;
    }

    status = name_object(*s, "framebuffer", true);
    if (!success(status)) {
        print("name_object returned {}\n", status);
        s->release();
        return;
    }
}

void create_other_framebuffer_section() {
    nt_status status;

    auto s = new section;
    if (!s) {
        print("new failed\n");
        return;
    }

    s->type = section_type::anonymous;
    s->size = (uint32_t)framebuffer.size_bytes();
    s->num_pages = bytes_to_pages(s->size);
    s->page_protection = PAGE_READWRITE;

    s->custom_cache = new framebuffer_cache<false>;
    if (!s->custom_cache) {
        print("new failed\n");
        s->release();
        return;
    }

    status = name_object(*s, "framebuffer", true);
    if (!success(status)) {
        print("name_object returned {}\n", status);
        s->release();
        return;
    }
}

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                         span<uint8_t> /*outbuf*/, uintptr_t& /*written*/) {
    switch (code) {
        case IOCTL_MOXNOX_FB_SET_CURSOR:
            return ioctl_set_cursor(inbuf);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::ioctl_set_cursor(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(MOXNOX_IOCTL_FB_SET_CURSOR))
        return nt_status::INVALID_PARAMETER;

    auto& sc = *(MOXNOX_IOCTL_FB_SET_CURSOR*)inbuf.data();
    auto& t = *get_current_thread();

    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    // FIXME - should be clearing existing cursor in this case?
    if (sc.width == 0 || sc.height == 0)
        return nt_status::INVALID_PARAMETER;

    if (!sc.data)
        return nt_status::INVALID_PARAMETER;

    if (sc.hotspot_x >= sc.width || sc.hotspot_y >= sc.height)
        return nt_status::INVALID_PARAMETER;

    // FIXME - if smaller than 64x64, should be copying and adding padding
    if (sc.width != 64 || sc.height != 64)
        return nt_status::INVALID_PARAMETER;

    size_t bytes = sc.width * sc.height * sizeof(uint32_t);

    auto buf = (uint32_t*)vmalloc(bytes);
    if (!buf) {
        print("vmalloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    if (!t.proc->copy_from(buf, sc.data, bytes)) {
        print("process::copy_from failed\n");
        vfree(buf, bytes);
        return nt_status::NO_MEMORY;
    }

    uint32_t* old_cursor_buffer;
    uint32_t old_cursor_width, old_cursor_height;

    {
        lock_guard lock(cursor_lock);

        old_cursor_buffer = cursor_buffer;
        old_cursor_width = cursor_width;
        old_cursor_height = cursor_height;

        cursor_buffer = buf;
        cursor_width = sc.width;
        cursor_height = sc.height;
        cursor_hotspot_x = sc.hotspot_x;
        cursor_hotspot_y = sc.hotspot_y;
    }

    if (old_cursor_buffer)
        vfree(old_cursor_buffer, old_cursor_width * old_cursor_height * sizeof(uint32_t));

    return nt_status::SUCCESS;
}
