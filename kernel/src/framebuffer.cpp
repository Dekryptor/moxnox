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

export span<uint32_t> shadow_framebuffer;

class framebuffer_cache : public custom_section_cache {
public:
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages);
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

export void create_framebuffer_section() {
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

    if (name_object(*s, "framebuffer", true)) {
        print("name_object failed\n");
        s->release();
        return;
    }
}
