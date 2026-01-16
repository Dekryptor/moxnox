module;

#include <optional>
#include <functional>

export module object_ref;

import objects;
import visit;

using namespace std;

// FIXME - extern "C++" only here because GCC 15 bugs out otherwise
export extern "C++" class object_ref {
public:
    object_ref() = default;

    object_ref(nullopt_t) : rw(nullopt) { }
    object_ref(object& obj) : rw(obj) { }

    object_ref(object_ref&& other) {
        rw = other.rw;
        other.rw = nullopt;
    }

    // FIXME - implement (and test) these (refcount issues?)
    object_ref(const object_ref& temp_obj) = delete;
    object_ref& operator=(const object_ref& temp_obj) = delete;

    object_ref& operator=(object_ref&& other) {
        rw = other.rw;
        other.rw = nullopt;
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
        optional<reference_wrapper<object>> rw2;

        rw2 = that.rw;
        that.rw = rw;
        this->rw = rw2;
    }

    void reset() {
        if (rw)
            object_release(rw->get());
    }

private:
    optional<reference_wrapper<object>> rw;
};
