#pragma once

enum class object_type {
    thread
};

class object {
public:
    object(enum object_type type) : type(type) { }

    enum object_type type;
    std::atomic<unsigned int> refcount = 1;
};

template<typename T>
concept is_sync_object = std::is_same_v<decltype(T::signalled), bool>;

#include "thread.h"

// FIXME - concept to make sure func is invocable
template<typename T>
static void visit_obj(object& obj, const T& func) {
    switch (obj.type) {
        case object_type::thread:
            func(static_cast<thread&>(obj));
        break;
    }
}
