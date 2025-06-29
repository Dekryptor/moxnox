module;

#include <variant>
#include "kernel.h"
#include "spinlock.h"
import page_cache;
import objects;
import rwsem;
#include "inode.h"
#include "section.h"
#include "handle.h"

export module framebuffer;

import print;
import mem;
import config;

using namespace std;

export span<uint32_t> framebuffer, shadow_framebuffer;
export uint32_t fb_width, fb_height, fb_pixels_per_scan_line, fb_dpi;

class framebuffer_cache : public custom_section_cache {
public:
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS flush_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                        span<uint8_t> buf, size_t* return_length) override;
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
