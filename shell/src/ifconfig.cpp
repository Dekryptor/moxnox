#include <stdint.h>
#include <string.h>
#include <optional>

import nt;
import print;
import unique_handle;
import moxnox_ioctls;

using namespace std;

static const char16_t netctl_path[] = u"/dev/netctl";

static unique_handle open_netctl() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (!success(status)) {
        print("NtCreateFile returned {}\n", status);
        return {};
    }

    return unique_handle{h};
}

static optional<uint32_t> get_netif_count(unique_handle& netctl) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    MOXNOX_NETCTL_GET_NETIF_COUNT out;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_COUNT, nullptr,
                                   0, &out, sizeof(out));
    if (!success(status)) {
        print("NtDeviceIoControlFile returned {}\n", status);
        return nullopt;
    }

    return out.count;
}

int main() {
    auto netctl = open_netctl();

    if (!netctl)
        return 1;

    uint32_t count;

    if (auto opt = get_netif_count(netctl); !opt.has_value())
        return 1;
    else
        count = *opt;

    print("count = {}\n", count);
}
