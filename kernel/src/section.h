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
    bool map(uint32_t offset, uint64_t va);
    bool copy_from(uint32_t offset, void* ks_addr, uint32_t size);
    uint64_t find_export(std::string_view name);

    std::optional<std::reference_wrapper<inode>> ino;
    enum section_type type;
    uint32_t size;
    uint32_t num_pages;
    uint64_t* pa = nullptr;
    uintptr_t preferred_base;
    uint64_t stack_size;
    spinlock alloc_lock;
};
