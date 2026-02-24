module;

#include <string_view>
#include <span>
#include <bit>
#include <new>
#include <stdint.h>

export module mem;

import config;
import list;
import format;
import spinlock;
import loaderblock;
export import mapping_type;

#ifdef __aarch64__
import aarch64;
#endif

#ifdef __x86_64__
// x86/mem.S
export {
    extern "C" void memcpy_aligned(void* dest, const void* src, size_t n);
    extern "C" void init_pat();
}
#endif

using namespace std;

export enum class page_state : uint8_t {
    free,
    used,
    reserved,
    folio_following,
    allocator_8k,
    allocator_4k,
    allocator_2k,
    allocator_1k,
    allocator_512,
    allocator_256,
    allocator_128,
    allocator_64,
    boot
};

export _spinlock<6> pa_spinlock;

export span<const uint8_t> initrd;

static const uint64_t MAX_PHYS_MEMORY_BITS = 44; // 16TB

constexpr uint64_t PHYS_MEMORY_MASK = ~(((uint64_t)1 << MAX_PHYS_MEMORY_BITS) - 1);
const uint64_t PHYS_MEMORY = 0xffff900000000000;

export constexpr uint8_t MAX_FOLIO_ORDER = 9; // 2MB

#ifdef __aarch64__

export uint8_t attr_index_normal;
export uintptr_t cache_line_shift;

#elifdef __x86_64__

export uintptr_t cache_line_shift = 6; // 64 bytes

#endif

export uint64_t zero_pfn;
export uint64_t num_ram_pages = 0;

export struct init_map_entry {
    list_entry le;
    uint64_t addr;
    uint64_t len;
    enum mem_type type;
};

export list_entry init_mappings = { &init_mappings, &init_mappings };

export inline void* phys_address(uint64_t pa) {
    return (void*)(PHYS_MEMORY + pa);
}

template<>
consteval bool format_spec_valid<enum page_state>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, enum page_state arg) {
    switch (arg) {
        case page_state::free:
            return do_format_arg(sp, "", "free");

        case page_state::used:
            return do_format_arg(sp, "", "used");

        case page_state::folio_following:
            return do_format_arg(sp, "", "folio_following");

        case page_state::allocator_8k:
            return do_format_arg(sp, "", "allocator_8k");

        case page_state::allocator_4k:
            return do_format_arg(sp, "", "allocator_4k");

        case page_state::allocator_2k:
            return do_format_arg(sp, "", "allocator_2k");

        case page_state::allocator_1k:
            return do_format_arg(sp, "", "allocator_1k");

        case page_state::allocator_512:
            return do_format_arg(sp, "", "allocator_512");

        case page_state::allocator_256:
            return do_format_arg(sp, "", "allocator_256");

        case page_state::allocator_128:
            return do_format_arg(sp, "", "allocator_128");

        case page_state::allocator_64:
            return do_format_arg(sp, "", "allocator_64");

        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

export constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

export constexpr bool is_phys_memory(const void* ptr) {
    return ((uintptr_t)ptr & PHYS_MEMORY_MASK) == PHYS_MEMORY;
}

export constexpr uint64_t phys_memory_pfn(const void* ptr) {
    return ((uintptr_t)ptr & ~PHYS_MEMORY_MASK) >> PAGE_SHIFT;
}

export constexpr uint64_t va_to_pa_phys(const void* ptr) {
    return (phys_memory_pfn(ptr) << PAGE_SHIFT) | ((uintptr_t)ptr & (PAGE_SIZE - 1));
}

export extern "C" void* malloc(size_t size);
export extern "C" void free(void* ptr);
export extern "C" void* calloc(size_t nmemb, size_t size);

export extern "C++" void* operator new(size_t count);
export extern "C++" void* operator new(size_t count, align_val_t al);
export extern "C++" void* operator new[](size_t count);
export extern "C++" void operator delete(void* ptr) noexcept;
export extern "C++" void operator delete(void* ptr, size_t) noexcept;
export extern "C++" void operator delete[](void* ptr) noexcept;
export extern "C++" void operator delete[](void* ptr, size_t) noexcept;

export uint64_t allocate_page(enum page_state state = page_state::used);
export void free_lower_half_mappings();
export void setup_folios();
export bool copy_map_ptr(const loader_block& lb);
export bool init_memory(const loader_block& lb);
export void free_no_lock(void* ptr);
export void free_folio(uint64_t pa);
export void free_bootloader_memory();
export uint64_t cow_folio(uint64_t pa);
export void unmap_page(uint64_t va);
export void protect_page(uint64_t va, enum mapping_type type);
export bool init_vablock();
export void free_va(uint64_t va, uint32_t num_pages);
export uint64_t va_to_pa_lookup(uint64_t va);
export uint8_t get_folio_order(uint64_t pa);
export void* vmalloc(size_t size);
export void vfree(void* ptr, size_t size);
export void increase_page_refcount(uint64_t pfn);
export enum mapping_type characteristics_to_mapping_type(uint32_t c);
export void protect_kernel_pages();

export template<bool no_lock = false>
void* malloc_aligned(size_t size, size_t align);

export template<bool no_lock = false>
uint64_t allocate_va(uint32_t num_pages);

export template<bool no_lock = false>
bool map_folio(uint64_t pa, uint64_t va, enum mapping_type type);

export template<bool no_lock = false>
void* allocate_stack();

export template<bool no_lock = false>
uint64_t allocate_folio(uint8_t order, enum page_state state = page_state::used);

export template<bool no_lock = false>
bool map_page(uint64_t pa, uint64_t va, enum mapping_type type);

#ifdef __aarch64__
export ARM64_HARDWARE_PTE* find_pte(uint64_t va);
export void mark_lower_half_writable();
#endif

export void inline __attribute__((always_inline)) barrier() {
#ifdef __aarch64__
    __asm __volatile (
        "dsb ish\n\t"
        "isb\n\t"
    );
#elifdef __x86_64__
    __asm__ __volatile__ (
        ""
        :
        :
        : "memory"
    );
#endif
}

export constexpr uint8_t bytes_to_folio_order(size_t s) {
    uint32_t p = bytes_to_pages(s);

    if (p == 0)
        return 0;

    return (uint8_t)(32 - countl_zero(p - 1));
}

#ifdef __aarch64__
export void inline __attribute((always_inline)) flush_cache(span<const uint8_t> va,
                                                            bool invalidate) {
    uint64_t cache_line_size = 1 << cache_line_shift;

    auto start = (uint64_t)va.data();
    auto len = va.size();

    if (start & (cache_line_size - 1)) {
        len += start & (cache_line_size - 1);
        start &= ~(cache_line_size - 1);
    }

    auto num = (len + cache_line_size - 1) >> cache_line_shift;

    __asm __volatile (
        ""
        :
        :
        : "memory"
    );

    if (invalidate) {
        for (unsigned int i = 0; i < num; i++) {
            __asm __volatile(
                "dc ivac, %x[res]\n"
                :
                : [res] "r" (start)
            );

            start += cache_line_size;
        }
    } else {
        for (unsigned int i = 0; i < num; i++) {
            __asm __volatile(
                "dc cvac, %x[res]\n"
                :
                : [res] "r" (start)
            );

            start += cache_line_size;
        }
    }

    __asm __volatile (
        "dsb sy\n"
        "isb\n"
        :
        :
        : "memory"
    );
}
#elifdef __x86_64__
export void inline __attribute((always_inline)) flush_cache(span<const uint8_t>, bool) {
    // FIXME?

    __asm __volatile (
        ""
        :
        :
        : "memory"
    );
}
#endif
