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

using namespace std;

export span<uint32_t> shadow_framebuffer;

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

    // FIXME - set pages

    if (name_object(*s, "framebuffer", true)) {
        print("name_object failed\n");
        s->release();
        return;
    }
}
