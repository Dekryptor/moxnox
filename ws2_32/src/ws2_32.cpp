#include <stdint.h>
#include <string.h>

import nt;
import ucrtbase;
import ws2_32;
import unique_handle;
import moxnox_ioctls;

static const char16_t netctl_path[] = u"/dev/netctl";

extern "C"
SOCKET socket(int af, int type, int protocol) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle netctl;
    MOXNOX_NETCTL_SOCKET params;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(netctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)netctl_path;

    auto status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0,
                               0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr,
                               0);
    if (success(status))
        netctl.reset(h);
    else {
        *_errno() = EINVAL;
        return INVALID_SOCKET;
    }

    params.af = af;
    params.type = type;
    params.protocol = protocol;
    params.h = &h;

    status = NtDeviceIoControlFile(netctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_NETCTL_SOCKET, &params,
                                   (uint32_t)sizeof(params), nullptr, 0);
    if (!success(status)) {
        *_errno() = EINVAL; // FIXME?
        return INVALID_SOCKET;
    }

    return (SOCKET)h;
}

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
