#include <stdint.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <string_view>

import nt;
import unique_handle;
import moxnox_ioctls;

#undef errno
import errno;

using namespace std;

static const char16_t netctl_path[] = u"/dev/netctl";

static nt_status open_netctl(unique_handle& ret) {
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
    if (!success(status))
        return status;

    ret = unique_handle{h};

    return nt_status::SUCCESS;
}

extern "C"
unsigned int if_nametoindex(const char* ifname) {
    nt_status status;
    unique_handle netctl;
    MOXNOX_NETCTL_GET_NETIF_BY_NAME p;
    IO_STATUS_BLOCK iosb;
    uint32_t id;

    if (!ifname) {
        *_errno() = EINVAL;
        return 0;
    }

    status = open_netctl(netctl);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return 0;
    }

    auto s = string_view(ifname);

    if (s.size() > sizeof(p.name)) {
        *_errno() = ENODEV;
        return 0;
    }

    memcpy(p.name, s.data(), s.size());
    memset(p.name + s.size(), 0, sizeof(p.name) - s.size());

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME, &p,
                                   sizeof(p), &id, (uint32_t)sizeof(id));
    if (status == nt_status::NOT_FOUND) {
        *_errno() = ENODEV;
        return 0;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return 0;
    }

    return id;
}

extern "C"
char* if_indextoname(unsigned int ifindex, char* ifname) {
    nt_status status;
    unique_handle netctl;
    MOXNOX_NETCTL_GET_NETIF_INFO info;
    IO_STATUS_BLOCK iosb;
    auto id = (uint32_t)ifindex;

    if (!ifname) {
        *_errno() = EINVAL;
        return nullptr;
    }

    status = open_netctl(netctl);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_GET_NETIF_INFO, &id,
                                   sizeof(uint32_t), &info,
                                   (uint32_t)sizeof(info));
    if (status == nt_status::NOT_FOUND) {
        *_errno() = ENXIO;
        return nullptr;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    strcpy(ifname, info.name);

    return ifname;
}

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
