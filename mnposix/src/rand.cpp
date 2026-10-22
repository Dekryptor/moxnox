// SPDX-License-Identifier: LGPL-2.1-or-later
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

import nt;
import unique_handle;

#undef errno
import errno;

extern "C"
int getentropy(void* buffer, size_t length) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle urandom;
    nt_status status;

    static const char16_t urandom_path[] = u"/dev/urandom";

    if (length > GETENTROPY_MAX) {
        *_errno() = EIO;
        return -1;
    }

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(urandom_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)urandom_path;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr, 0,
                          0, FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);
    if (success(status))
        urandom.reset(h);
    else {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    while (true) {
        status = NtReadFile(urandom.get(), 0, nullptr, nullptr, &iosb, buffer,
                            (uint32_t)length, nullptr, nullptr);
        if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        } else if (status == nt_status::USER_APC)
            continue;

        if (iosb.Information == length)
            break;

        buffer = (uint8_t*)buffer + iosb.Information;
        length -= iosb.Information;
    }

    return 0;
}
