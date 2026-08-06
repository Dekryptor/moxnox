#include <stdint.h>
#include <string.h>
#include <string_view>
#include <getopt.h>

import nt;
import print;
import unique_handle;
import moxnox_ioctls;

using namespace std;

static const char16_t tracectl_path[] = u"/dev/tracectl";

static unique_handle open_tracectl() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(tracectl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)tracectl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return {};
    }

    return unique_handle{h};
}

static int enable_tracepoint(unique_handle& tracectl, string_view name) {
    nt_status status;
    IO_STATUS_BLOCK iosb;

    status = NtDeviceIoControlFile(tracectl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_TRACECTL_ENABLE, name.data(),
                                   (uint32_t)name.size(), nullptr, 0);
    if (status == nt_status::NOT_FOUND) {
        print_stderr("tracepoint {} not found\n", name);
        return 1;
    } else if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return 1;
    }

    return 0;
}

static int disable_tracepoint(unique_handle& tracectl, string_view name) {
    nt_status status;
    IO_STATUS_BLOCK iosb;

    status = NtDeviceIoControlFile(tracectl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_TRACECTL_DISABLE, name.data(),
                                   (uint32_t)name.size(), nullptr, 0);
    if (status == nt_status::NOT_FOUND) {
        print_stderr("tracepoint {} not found\n", name);
        return 1;
    } else if (!success(status)) {
        print_stderr("NtDeviceIoControlFile returned {}\n", status);
        return 1;
    }

    return 0;
}

int main(int argc, char** argv) {
    bool enable = false, disable = false;

    // FIXME - listing tracepoints

    static const option options[] = {
        { "enable", no_argument, nullptr, 'e' },
        { "disable", no_argument, nullptr, 'e' },
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "de", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case 'd':
                disable = true;
                break;

            case 'e':
                enable = true;
                break;

            case '?':
                return 1;
        }
    }

    if (enable && disable) {
        print_stderr("--enable and --disable are mutually exclusive\n");
        return 1;
    }

    if (argc != optind + 1) {
        print_stderr("usage: trace [--disable|--enable|-d|-e] <name>\n");
        return 1;
    }

    if (!enable && !disable)
        enable = true;

    auto tracectl = open_tracectl();

    if (!tracectl)
        return 1;

    auto name = argv[optind];

    if (enable)
        return enable_tracepoint(tracectl, name);
    else if (disable)
        return disable_tracepoint(tracectl, name);
}
