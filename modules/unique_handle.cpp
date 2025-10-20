export module unique_handle;

import nt;

export class __attribute__ ((warn_unused)) unique_handle {
public:
    unique_handle() : h(0) {
    }

    explicit unique_handle(HANDLE h) : h(h) {
    }

    unique_handle(unique_handle&& that) noexcept {
        h = that.h;
        that.h = 0;
    }

    unique_handle(const unique_handle&) = delete;
    unique_handle& operator=(const unique_handle&) = delete;

    unique_handle& operator=(unique_handle&& that) noexcept {
        if (h > 0)
            NtClose(h);

        h = that.h;
        that.h = 0;

        return *this;
    }

    ~unique_handle() {
        if (h <= 0)
            return;

        NtClose(h);
    }

    explicit operator bool() const noexcept {
        return h != 0;
    }

    void reset(HANDLE new_h = 0) noexcept {
        if (h > 0)
            NtClose(h);

        h = new_h;
    }

    HANDLE get() const noexcept {
        return h;
    }

private:
    HANDLE h;
};
