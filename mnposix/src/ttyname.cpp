// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <string_view>
#include <span>

import nt;
import ucrtbase;
import unicode;

#undef errno
import errno;

using namespace std;

extern "C"
int ttyname_r(int fd, char* name, size_t namesize) {
    alignas(OBJECT_NAME_INFORMATION) char buf[sizeof(OBJECT_NAME_INFORMATION) + (100 * sizeof(char16_t))];
    uint32_t retlen;

    if (!isatty(fd))
        return ENOTTY;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return EBADF;

    auto status = NtQueryObject(h, OBJECT_INFORMATION_CLASS::NameInformation,
                                buf, sizeof(buf), &retlen);
    if (!success(status))
        return nt_status_to_errno(status);

    auto& oni = *(OBJECT_NAME_INFORMATION*)buf;
    auto u16sv = u16string_view(oni.Name.Buffer, oni.Name.Length / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(u16sv);

    if (namesize < u8len + 1)
        return ERANGE;

    utf16_to_utf8(span(name, namesize), u16sv);
    name[u8len] = 0;

    return 0;
}

extern "C"
char* ttyname(int fd) {
    static char buf[100];

    auto ret = ttyname_r(fd, buf, sizeof(buf));

    if (!ret)
        return buf;
    else {
        *_errno() = ret;
        return nullptr;
    }
}
