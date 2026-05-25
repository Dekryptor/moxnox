#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string_view>
#include <span>

import nt;
import unicode;

#undef errno
import errno;

using namespace std;

struct _DIR {
    HANDLE h;
};

extern "C"
DIR* opendir(const char* name) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    UNICODE_STRING us;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    nt_status status;
    HANDLE h;

    if (!name || name[0] == 0) {
        *_errno() = EINVAL;
        return nullptr;
    }

    auto sv = string_view(name);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return nullptr;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb.ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_LIST_DIRECTORY, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    free(u16pathname);

    auto d = (_DIR*)malloc(sizeof(_DIR));
    if (!d) {
        NtClose(h);
        *_errno() = ENOMEM;
        return nullptr;
    }

    d->h = h;

    return (DIR*)d;
}
