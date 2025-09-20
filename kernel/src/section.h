#pragma once

#include <variant>
#include <string_view>
#include "spinlock.h"

import page_cache;

enum class section_type {
    anonymous,
    file,
    image
};

struct section_info {
    uint64_t offset;
    uint64_t size;
};

struct dwarf_info {
    section_info debug_info;
    section_info debug_abbrev;
    section_info debug_rnglists;
    section_info debug_str;
    section_info debug_addr;
    section_info debug_str_offsets;
};

struct IMAGE_SECTION_HEADER;

class anon_section_cache {
public:
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages);

    page_cache<anon_section_cache> cache{*this};
};

static_assert(cacheable<anon_section_cache>);

class custom_section_cache {
public:
    virtual size_t read_into_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                                std::span<uint8_t> buf, size_t* return_length) = 0;
    virtual NTSTATUS flush_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual nt_status ioctl(uint32_t code, std::span<const uint8_t> inbuf,
                            std::span<uint8_t> outbuf, uintptr_t& written) = 0;
    virtual ~custom_section_cache() { }

    page_cache<custom_section_cache> cache{*this};
};

static_assert(cacheable<custom_section_cache>);
static_assert(cacheable_flushable<custom_section_cache>);
static_assert(has_ioctls<custom_section_cache>);

class image_section_cache;
class inode;

using cache_variant = std::variant<page_cache<image_section_cache>*, page_cache<anon_section_cache>*,
                                   page_cache<custom_section_cache>*, page_cache<inode>*>;

class section : public object {
public:
    section() : object(object_type::section) { }
    ~section();

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status load_image();
    uint64_t find_export(std::string_view name);
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class, std::span<uint8_t> buf,
                        size_t* return_length);
    cache_variant get_cache();
    nt_status flush(uint64_t page_num, uint64_t num_pages);
    nt_status ioctl(uint32_t code, std::span<const uint8_t> inbuf,
                    std::span<uint8_t> outbuf, uintptr_t& written);

    std::optional<std::reference_wrapper<inode>> ino;
    enum section_type type;
    uint32_t size;
    uint32_t num_pages;
    uintptr_t preferred_base, entry_point;
    uint16_t characteristics, machine;
    uint64_t stack_size;
    spinlock alloc_lock;
    char* name = nullptr;
    size_t namelen = 0;
    dwarf_info dwinfo;
    unsigned int num_pe_sections = 0;
    IMAGE_SECTION_HEADER* pe_sections = nullptr;
    uint32_t size_of_headers;
    image_section_cache* image_cache = nullptr;
    anon_section_cache* anon_cache = nullptr;
    custom_section_cache* custom_cache = nullptr;
    uint32_t page_protection;
};

static_assert(has_ioctls<section>);
