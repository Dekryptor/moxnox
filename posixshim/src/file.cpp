#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <span>

import nt;
import unicode;

#undef errno
import errno;

using namespace std;

extern "C"
int mkdir(const char* path, mode_t /*mode*/) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file;
    IO_STATUS_BLOCK iosb;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    auto pathsv = string_view(path);

    if (pathsv.empty()) {
        *_errno() = EINVAL;
        return -1;
    }

    auto u16len = utf8_to_utf16_len(pathsv);
    auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));

    if (!u16) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16, u16len), pathsv);

    // FIXME - mode?

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)u16;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&file, 0, &oa, &iosb, nullptr, FILE_ATTRIBUTE_DIRECTORY,
                          0, FILE_CREATE, FILE_DIRECTORY_FILE, nullptr, 0);

    free(u16);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        *_errno() = EEXIST;
        return -1;
    } else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        *_errno() = ENOENT;
        return -1;
    } else if (!success(status)) {
        *_errno() = EINVAL;
        return -1;
    }

    NtClose(file);

    return 0;
}
