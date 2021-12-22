#pragma once

enum class object_type {
    thread,
    timer,
    event,
    pipe
};

class object {
public:
    object(enum object_type type) : type(type) { }

    enum object_type type;
    std::atomic<unsigned int> refcount = 1;
    std::atomic<unsigned int> handle_count = 0;
    bool named = false;
};

template<typename T>
concept is_sync_object = std::is_same_v<decltype(T::signalled), bool>;

template<typename T>
concept is_writable_object = requires(T t, std::span<const uint8_t> s) {
    { t.write(s) };
};

// FIXME - version of this specifying offset
// FIXME - async reads and writes (specifying event)
template<typename T>
concept is_readable_object = requires(T t, std::span<uint8_t> s) {
    { t.read(s) } -> std::convertible_to<size_t>;
};

#include "thread.h"
#include "timer.h"
#include "event.h"
#include "pipe.h"

// FIXME - concept to make sure func is invocable
template<typename T>
static void visit_obj(object& obj, const T& func) {
    switch (obj.type) {
        case object_type::thread:
            func(static_cast<thread&>(obj));
        break;

        case object_type::timer:
            func(static_cast<timer&>(obj));
        break;

        case object_type::event:
            func(static_cast<event&>(obj));
        break;

        case object_type::pipe:
            func(static_cast<pipe&>(obj));
        break;
    }
}
