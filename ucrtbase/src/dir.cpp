#include <stdint.h>
#include <stdlib.h>
#include <string_view>
#include <span>

import nt;
import ucrtbase;
import unicode;
import print;

#undef errno
import errno;

using namespace std;

extern "C"
char* _getcwd(char* buffer, int maxlen) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;
    const auto& teb = *NtCurrentTeb();
    const auto& peb = *teb.ProcessEnvironmentBlock;
    const auto& params = *peb.ProcessParameters;

    if (maxlen < 0) {
        *_errno() = ERANGE;
        return nullptr;
    }

    status = NtQueryInformationFile(params.CurrentDirectory.Handle, &iosb,
                                    &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        *_errno() = ENOMEM;
        return nullptr;
    }

    status = NtQueryInformationFile(params.CurrentDirectory.Handle, &iosb,
                                    fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        free(fni2);
        *_errno() = nt_status_to_errno(status);
        return nullptr;
    }

    auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(sv);

    if ((size_t)maxlen < u8len + 1) {
        free(fni2);
        *_errno() = ERANGE;
        return nullptr;
    }

    utf16_to_utf8(span(buffer, maxlen), sv);
    buffer[u8len] = 0;

    free(fni2);

    return buffer;
}

extern "C"
int _chdir(const char* path) {
    print("FIXME - _chdir \"{}\"\n", path);

    *_errno() = EINVAL;
    return -1;
}
