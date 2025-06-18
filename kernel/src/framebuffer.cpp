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

static void populate_cache(anon_section_cache& cache) {
    uint64_t num_pages = bytes_to_pages(shadow_framebuffer.size_bytes());

    // FIXME - make sure these don't get unpinned

    cache.cache.pin(0, num_pages, nullptr);

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lock(cache.cache.cache_lock);

            if (i == 0)
                p = cache.cache.find_page_cache_entry(0);
            else
                p = cache.cache.next_page_cache_entry(*p);
        }

        auto va = (uintptr_t)((uint8_t*)shadow_framebuffer.data() + (i << PAGE_SHIFT));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = va_to_pa_lookup(va) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    cache.cache.unpin(0, num_pages);
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

    s->anon_cache = new anon_section_cache;
    if (!s->anon_cache) {
        print("new failed\n");
        s->release();
        return;
    }

    populate_cache(*s->anon_cache);

    if (name_object(*s, "framebuffer", true)) {
        print("name_object failed\n");
        s->release();
        return;
    }
}
