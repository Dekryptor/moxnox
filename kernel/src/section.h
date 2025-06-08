#pragma once

#include "spinlock.h"

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

class section;

class section_cache : public cacheable {
public:
    section_cache(section& sect) : sect(sect) { }
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;

private:
    section& sect;
};

class image_section_cache : public cacheable {
public:
    image_section_cache(section& sect) : sect(sect) { }
    virtual ~image_section_cache() { }

    size_t read_into_cache(uint64_t page_num, uint64_t num_pages) override;
    size_t flush_cache(uint64_t page_num, uint64_t num_pages) override;

private:
    section& sect;
};

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
    page_cache& get_cache();

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
    section_cache cache{*this};
    image_section_cache* image_cache = nullptr;
};
