#pragma once

class inode : public object {
public:
    inode() : object(object_type::inode) { }

    virtual ~inode() {
    }

    void acquire() {
        refcount++;
    }

    virtual void release() = 0;
};
