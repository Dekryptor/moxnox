#pragma once

#include "spinlock.h"

enum class section_type {
    anonymous,
    file,
    image
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

    bool load_image();
    void free_pa();
    bool map(uint32_t offset, uint64_t va, bool readonly);
    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
    bool copy_to(uint32_t offset, const void* ks_addr, uint32_t size);
    uint64_t find_export(std::string_view name);
    NTSTATUS protect(uint32_t offset, uint32_t page_count, enum mapping_type prot,
                     enum mapping_type& old_prot);
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class, std::span<uint8_t> buf,
                        size_t* return_length);

    std::optional<std::reference_wrapper<inode>> ino;
    enum section_type type;
    uint32_t size;
    uint32_t num_pages;
    page_info* pages = nullptr;
    uintptr_t preferred_base, entry_point;
    uint64_t stack_size;
    spinlock alloc_lock;
    char* name = nullptr;
    size_t namelen = 0;
};
