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
    bool first;
};

static dirent de;

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
    d->first = true;

    return (DIR*)d;
}

extern "C"
dirent* readdir(DIR* dir) {
    auto& d = *reinterpret_cast<_DIR*>(dir);
    IO_STATUS_BLOCK iosb;
    char buf[4096];
    nt_status status;

    // FIXME - get multiple entries at once and cache them?

    status = NtQueryDirectoryFile(d.h, 0, nullptr, nullptr, &iosb, buf, sizeof(buf),
                                  FILE_INFORMATION_CLASS::DirectoryInformation,
                                  true, nullptr, d.first);
    if (status == nt_status::NO_MORE_FILES)
        return nullptr;
    else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    auto& fdi = *(FILE_DIRECTORY_INFORMATION*)buf;
    auto u16sv = u16string_view(fdi.FileName, fdi.FileNameLength / sizeof(char16_t));
    auto u16len = utf16_to_utf8_len(u16sv);

    de.d_ino = 0; // FIXME!

    if (fdi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        de.d_type = DT_LNK;
    else if (fdi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        de.d_type = DT_DIR;
    else
        de.d_type = DT_REG;

    utf16_to_utf8(span(de.d_name, sizeof(de.d_name) - 1), u16sv);

    if (u16len < sizeof(de.d_name))
        de.d_name[u16len] = 0;
    else
        de.d_name[sizeof(de.d_name) - 1] = 0;

    d.first = false;

    return &de;
}

extern "C"
int closedir(DIR* dir) {
    auto& d = *reinterpret_cast<_DIR*>(dir);

    // FIXME - return errors from NtClose?

    NtClose(d.h);
    free(dir);

    return 0;
}
