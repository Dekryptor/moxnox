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

    std::optional<std::reference_wrapper<inode>> ino;
};
