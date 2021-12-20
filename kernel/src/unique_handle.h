#pragma once

class unique_handle {
public:
    unique_handle() : h(0) { }
    explicit unique_handle(handle_t h) : h(h) { }

    unique_handle(unique_handle&& that) noexcept {
        h = that.h;
        that.h = 0;
    }

    unique_handle(const unique_handle&) = delete;
    unique_handle& operator=(const unique_handle&) = delete;

    unique_handle& operator=(unique_handle&& that) noexcept {
        if (h)
            close_handle(h);

        h = that.h;
        that.h = 0;

        return *this;
    }

    ~unique_handle() {
        if (h)
            close_handle(h);
    }

    handle_t get() noexcept {
        return h;
    }

    void reset(handle_t new_h) noexcept {
        if (h)
            close_handle(h);

        h = new_h;
    }

private:
    handle_t h;
};
