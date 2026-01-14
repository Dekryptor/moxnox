module;

#include <stdint.h>
#include <span>

export module dma;

import mem;
import crt;
import print;
import config;

using namespace std;

static bool is_cache_line_aligned(span<const uint8_t> buf) {
    if (buf.size() & ((1 << cache_line_shift) - 1))
        return false;

    if ((uintptr_t)buf.data() & ((1 << cache_line_shift) - 1))
        return false;

    return true;
}

static bool need_bounce(span<const uint8_t> buf) {
    if (!is_cache_line_aligned(buf))
        return true;

    if (is_phys_memory(buf.data()))
        return false;

    auto page_start = (uintptr_t)buf.data() & ~(PAGE_SIZE - 1);
    auto page_end = (uintptr_t)(buf.data() + buf.size() - 1) & ~(PAGE_SIZE - 1);

    if (page_start != page_end)
        return true;

    return false;
}

export uint64_t dma_setup(span<const uint8_t> buf, bool write, uint8_t*& bounce) {
    if (need_bounce(buf)) {
        size_t bounce_size = buf.size();

        if (bounce_size & ((1 << cache_line_shift) - 1)) {
            bounce_size &= ~(((uint64_t)1 << cache_line_shift) - 1);
            bounce_size += 1 << cache_line_shift;
        }

        bounce = (uint8_t*)malloc(bounce_size);
        if (!bounce) {
            print("malloc failed\n");
            return 0;
        }

        if (write)
            memcpy(bounce, buf.data(), buf.size());

        flush_cache(span(bounce, buf.size()), false);

        return va_to_pa_phys(bounce);
    } else {
        flush_cache(span(buf.data(), buf.size()), false);

        if (is_phys_memory(buf.data()))
            return va_to_pa_phys(buf.data());
        else
            return va_to_pa_lookup((uintptr_t)buf.data());
    }
}

export void dma_finish_read(span<uint8_t> buf, uint8_t* bounce) {
    if (bounce) {
        flush_cache(span(bounce, buf.size()), true);
        memcpy(buf.data(), bounce, buf.size());

        free(bounce);
    } else
        flush_cache(buf, true);
}

export void dma_finish_write(uint8_t* bounce) {
    if (bounce)
        free(bounce);
}
