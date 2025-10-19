module;

#include <variant>
#include <ioctl.h>
#include <nt.h>
#include <span>
#include <optional>
#include <functional>
#include <string_view>
#include <mutex>
#include "section.h"
#include "handle.h"

export module framebuffer;

import print;
import mem;
import config;
import inout;
import spinlock;

using namespace std;

export span<uint32_t> framebuffer, shadow_framebuffer;
export uint32_t fb_width, fb_height, fb_pixels_per_scan_line, fb_dpi;

export span<uint16_t> text_console;
export uint32_t text_console_cols, text_console_rows;
static span<uint16_t> shadow_text_console;

export spinlock cursor_lock;

class framebuffer_cache : public custom_section_cache {
public:
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS flush_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                        span<uint8_t> buf, size_t* return_length) override;

    nt_status ioctl(uint32_t, span<const uint8_t>, span<uint8_t>,
                    uintptr_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

class text_console_cache : public custom_section_cache {
public:
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS flush_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                        span<uint8_t> buf, size_t* return_length) override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;

private:
    nt_status set_cursor(span<const uint8_t> inbuf, uintptr_t& written);
    bool cursor_enabled = true;
};

size_t framebuffer_cache::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    if (!cache.pin(page_num, num_pages, nullptr)) {
        print("page_cache::pin failed\n");
        return 0;
    }

    page_cache_entry* p;
    uint64_t read_pages = num_pages;

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

        auto va = (uintptr_t)((uint8_t*)shadow_framebuffer.data() + ((page_num + i) << PAGE_SHIFT));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = va_to_pa_lookup(va) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    cache.unpin(page_num, num_pages);

    return read_pages;
}

NTSTATUS framebuffer_cache::flush_cache(uint64_t page_num, uint64_t num_pages) {
    auto start = page_num << PAGE_SHIFT;

    if (start >= shadow_framebuffer.size_bytes())
        return nt_status::SUCCESS;

    auto len = num_pages << PAGE_SHIFT;

    if (start + len > shadow_framebuffer.size_bytes())
        len = shadow_framebuffer.size_bytes() - start;

    memcpy((uint8_t*)framebuffer.data() + start,
           (uint8_t*)shadow_framebuffer.data() + start,
           len);

    return nt_status::SUCCESS;
}

NTSTATUS framebuffer_cache::query_info(enum SECTION_INFORMATION_CLASS section_information_class,
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

export void create_framebuffer_section() {
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

    s->custom_cache = new framebuffer_cache;
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

size_t text_console_cache::read_into_cache(uint64_t page_num, uint64_t num_pages) {
    if (!cache.pin(page_num, num_pages, nullptr)) {
        print("page_cache::pin failed\n");
        return 0;
    }

    page_cache_entry* p;
    uint64_t read_pages = num_pages;

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

        auto va = (uintptr_t)((uint8_t*)shadow_text_console.data() + ((page_num + i) << PAGE_SHIFT));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = va_to_pa_lookup(va) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    cache.unpin(page_num, num_pages);

    return read_pages;
}

NTSTATUS text_console_cache::flush_cache(uint64_t page_num, uint64_t num_pages) {
    auto start = page_num << PAGE_SHIFT;

    if (start >= shadow_text_console.size_bytes())
        return nt_status::SUCCESS;

    auto len = num_pages << PAGE_SHIFT;

    if (start + len > shadow_text_console.size_bytes())
        len = shadow_text_console.size_bytes() - start;

    memcpy((uint8_t*)text_console.data() + start,
           (uint8_t*)shadow_text_console.data() + start,
           len);

    return nt_status::SUCCESS;
}

NTSTATUS text_console_cache::query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                                       span<uint8_t> buf, size_t* return_length) {
    switch (section_information_class) {
        case SECTION_INFORMATION_CLASS::MoxnoxTextConsoleInformation: {
            if (buf.size() < sizeof(MN_SECTION_TEXT_CONSOLE_INFORMATION))
                return nt_status::BUFFER_TOO_SMALL;

            auto& info = *(MN_SECTION_TEXT_CONSOLE_INFORMATION*)buf.data();

            info.cols = text_console_cols;
            info.rows = text_console_rows;

            *return_length = sizeof(MN_SECTION_TEXT_CONSOLE_INFORMATION);

            return nt_status::SUCCESS;
        }

        default:
            return nt_status::INVALID_INFO_CLASS;
    }
}

nt_status text_console_cache::set_cursor(span<const uint8_t> inbuf,
                                         uintptr_t& written) {
    if (inbuf.size() < sizeof(MOXNOX_SET_CURSOR))
        return nt_status::INVALID_PARAMETER;

    const auto& mnsc = *(MOXNOX_SET_CURSOR*)inbuf.data();

    // FIXME - disabling

    if (mnsc.x >= text_console_cols || mnsc.y >= text_console_rows)
        return nt_status::INVALID_PARAMETER;

    if (!!mnsc.enabled != cursor_enabled) {
        lock_guard lock(cursor_lock);

        if (mnsc.enabled) {
            uint8_t cursor_start = 14, cursor_end = 15;

            outb(0x3d4, 0x0a);
            outb(0x3d5, (inb(0x3d5) & 0xc0) | cursor_start);

            outb(0x3d4, 0x0b);
            outb(0x3d5, (inb(0x3d5) & 0xe0) | cursor_end);
        } else {
            outb(0x3d4, 0xa);
            outb(0x3d5, 0x20);
        }

        cursor_enabled = mnsc.enabled;
    }

    auto pos = (uint16_t)(((uint8_t)mnsc.y * text_console_cols) + (uint8_t)mnsc.x);

    {
        lock_guard lock(cursor_lock);

        outb(0x3d4, 0xf);
        outb(0x3d5, (uint8_t)(pos & 0xff));
        outb(0x3d4, 0xe);
        outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
    }

    written = 0;

    return nt_status::SUCCESS;
}

nt_status text_console_cache::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                    span<uint8_t>, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_CONSOLE_SET_CURSOR:
            return set_cursor(inbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

export void create_text_console_section() {
    nt_status status;

    auto ptr = (uint16_t*)vmalloc(text_console_cols * text_console_rows * sizeof(uint16_t));
    if (!ptr) {
        print("vmalloc failed\n");
        return;
    }

    shadow_text_console = span(ptr, text_console_cols * text_console_rows);

    memcpy(shadow_text_console.data(), text_console.data(),
           text_console.size_bytes());

    auto s = new section;
    if (!s) {
        print("new failed\n");
        return;
    }

    s->type = section_type::anonymous;

    s->num_pages = bytes_to_pages(shadow_text_console.size_bytes());
    if (shadow_text_console.size_bytes() & (PAGE_SIZE - 1))
        s->num_pages++;

    s->size = s->num_pages << PAGE_SHIFT;
    s->page_protection = PAGE_READWRITE;

    s->custom_cache = new text_console_cache;
    if (!s->custom_cache) {
        print("new failed\n");
        s->release();
        return;
    }

    status = name_object(*s, "text_console", true);
    if (!success(status)) {
        print("name_object returned {}\n", status);
        s->release();
        return;
    }
}
