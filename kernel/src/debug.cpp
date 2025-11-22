module;

#include <string_view>
#include <stdint.h>
#include <span>

export module debug;

import cmdline;
import print;
import object_ref;
import handle;
import nt;
import objects;
import bsod;
import thread;
import visit;
import crt;

using namespace std;

static object_ref debug_chardev;

static void debug_thread() {
    uint8_t buf[255];

    print("FIXME - debug\n");

    while (true) {
        uint64_t read_bytes;

        auto status = object_read_without_offset(debug_chardev.get(), span(buf),
                                                 read_bytes);
        if (!success(status)) {
            print("debug_thread: object_read_without_offset returned {}\n", status);
            break;
        }

        if (read_bytes == 0)
            continue;

        span sp(buf, read_bytes);

        print("message: \"{}\"\n", string_view((char*)sp.data(), sp.size()));
    }
}

export void init_debug() {
    string_view debug_opt;

    loop_through_cmdline([&](string_view s) {
        static const string_view prefix = "debug=";

        if (s.size() >= prefix.size() && string_view(s.data(), prefix.size()) == prefix) {
            debug_opt = string_view(s.data() + prefix.size(), s.size() - prefix.size());
            return false;
        }

        return true;
    });

    if (debug_opt.empty())
        return;

    static const char dev_prefix[] = "/dev/";

    if (debug_opt.size() < sizeof(dev_prefix) - 1 ||
        memcmp(debug_opt.data(), dev_prefix, sizeof(dev_prefix) - 1)) {
        print("invalid debug cmdline option\n");
        return;
    }

    auto sv = string_view(debug_opt.data() + sizeof(dev_prefix) - 1,
                          debug_opt.size() - sizeof(dev_prefix) + 1);

    auto status = open_object(sv, debug_chardev);
    if (!success(status))
        print("open_object returned {} trying to open debug object {}", status, debug_opt);

    if (debug_chardev.get().type != object_type::chardev)
        bsod_format("debug option {} did not point to a character device", debug_opt);

    // FIXME - make it so no-one else can open the chardev

    // FIXME - need to mark thread as priority?
    add_kernel_thread(debug_thread);
}
