#pragma once

#include "objects.h"

class object_ref {
public:
    object_ref() = default;

    object_ref(std::nullopt_t) : rw(std::nullopt) { }
    object_ref(object& obj) : rw(obj) { }

    // FIXME - implement (and test) these (refcount issues?)
    object_ref(object_ref& temp_obj) = delete;
    object_ref& operator=(const object_ref& temp_obj) = delete;

    object_ref& operator=(object_ref&& other) {
        rw = other.rw;
        other.rw = std::nullopt;
        return *this;
    }

    ~object_ref() {
        if (rw) {
            visit_obj(rw->get(), [](auto&& o) {
                o.release();
            });
        }
    }

    explicit operator bool() const {
        return rw.has_value();
    }

    object& get() {
        return rw->get();
    }

private:
    std::optional<std::reference_wrapper<object>> rw;
};

handle_t add_kernel_handle(object& obj, uint32_t access_mask);
void close_handle(handle_t h);
std::optional<std::reference_wrapper<object>> deref_handle(handle_t h);
object_ref deref_handle2(handle_t h);
bool name_object(handle_t h, std::string_view name, bool persistent = false);
bool name_object(object& obj, std::string_view name, bool persistent = false);
object_ref open_object(std::string_view name);
size_t read(handle_t h, std::span<uint8_t> s, std::optional<size_t> offset = std::nullopt);
void write(handle_t h, std::span<const uint8_t> s);
void write(handle_t h, std::string_view s);
void handle_closed(object& obj);
