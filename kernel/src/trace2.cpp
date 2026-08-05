module;

#include <stdint.h>
#include <span>
#include <string_view>

module trace;

import nt;
import chardev;
import print;
import handle;
import object_ref;
import moxnox_ioctls;

using namespace std;

class tracectl : public chardev {
public:
    tracectl() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
};

bool create_dev_tracectl() {
    auto pptr = new tracectl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "tracectl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

static nt_status enable_tracepoint(string_view name) {
    if (name.empty())
        return nt_status::INVALID_PARAMETER;

    print("FIXME - enable tracepoint \"{}\"\n", name);

    return nt_status::SUCCESS;
}

nt_status tracectl::ioctl(uint32_t code, span<const uint8_t> inbuf,
                          span<uint8_t> /*outbuf*/, uintptr_t& /*written*/) {
    switch (code) {
        case IOCTL_MOXNOX_TRACECTL_ENABLE:
            return enable_tracepoint(string_view((char*)inbuf.data(), inbuf.size()));

        default:
            return nt_status::NOT_SUPPORTED;
    }
}
