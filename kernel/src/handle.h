#pragma once

class object;

class object_ref {
public:
    object_ref() = default;

    object_ref(std::nullopt_t) : rw(std::nullopt) { }
    object_ref(object& obj) : rw(obj) { }

    object_ref(object_ref&& other) {
        rw = other.rw;
        other.rw = std::nullopt;
    }

    // FIXME - implement (and test) these (refcount issues?)
    object_ref(const object_ref& temp_obj) = delete;
    object_ref& operator=(const object_ref& temp_obj) = delete;

    object_ref& operator=(object_ref&& other) {
        rw = other.rw;
        other.rw = std::nullopt;
        return *this;
    }

    ~object_ref() {
        reset();
    }

    explicit operator bool() const {
        return rw.has_value();
    }

    object& get() {
        return rw->get();
    }

    void swap(object_ref& that) {
        std::optional<std::reference_wrapper<object>> rw2;

        rw2 = that.rw;
        that.rw = rw;
        this->rw = rw2;
    }

    void reset();

private:
    std::optional<std::reference_wrapper<object>> rw;
};

object_ref deref_handle(handle_t h);
object_ref deref_handle(handle_t h, ACCESS_MASK& access_mask);
nt_status name_object(object& obj, std::string_view name, bool persistent = true);
nt_status open_object(std::string_view name, object_ref& obj);
void handle_closed(object& obj);
