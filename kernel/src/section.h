#pragma once

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

    std::optional<std::reference_wrapper<inode>> ino;
    size_t size;
    uint32_t num_pages;
    uint64_t* pa = nullptr;
};
