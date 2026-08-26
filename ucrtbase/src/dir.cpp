#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <string_view>
#include <span>
#include <mutex>

import nt;
import ntdll;
import ucrtbase;
import unicode;
import print;

#undef errno
import errno;

using namespace std;

extern "C" int* __errno();

extern "C"
char* _getcwd(char* buffer, int maxlen) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;
    const auto& teb = *NtCurrentTeb();
    const auto& peb = *teb.ProcessEnvironmentBlock;
    const auto& params = *peb.ProcessParameters;

    if (buffer && maxlen == 0) {
        *__errno() = EINVAL;
        return nullptr;
    }

    if (maxlen < 0) {
        *__errno() = ERANGE;
        return nullptr;
    }

    status = NtQueryInformationFile(params.CurrentDirectory.Handle, &iosb,
                                    &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        *__errno() = nt_status_to_errno(status);
        return nullptr;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        *__errno() = ENOMEM;
        return nullptr;
    }

    status = NtQueryInformationFile(params.CurrentDirectory.Handle, &iosb,
                                    fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        free(fni2);
        *__errno() = nt_status_to_errno(status);
        return nullptr;
    }

    auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(sv);

    if (!buffer && maxlen == 0)
        maxlen = (int)(u8len + 1);

    if ((size_t)maxlen < u8len + 1) {
        free(fni2);
        *__errno() = ERANGE;
        return nullptr;
    }

    if (!buffer) {
        buffer = (char*)malloc(maxlen);
        if (!buffer) {
            free(fni2);
            *__errno() = ENOMEM;
            return nullptr;
        }
    }

    utf16_to_utf8(span(buffer, (size_t)maxlen), sv);
    buffer[u8len] = 0;

    free(fni2);

    return buffer;
}

bool get_dos_path(HANDLE h, UNICODE_STRING& us) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;

    status = NtQueryInformationFile(h, &iosb, &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        *__errno() = nt_status_to_errno(status);
        return false;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        *__errno() = ENOMEM;
        return false;
    }

    status = NtQueryInformationFile(h, &iosb, fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        free(fni2);
        return false;
    }

    auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    static const u16string_view prefix = u"C:";

    us.Length = min(us.MaximumLength, (uint16_t)((sv.size() + prefix.size()) * sizeof(char16_t)));
    memcpy(us.Buffer, prefix.data(), prefix.size() * sizeof(char16_t));
    memcpy(us.Buffer + prefix.size(), sv.data(), us.Length - (prefix.size() * sizeof(char16_t)));

    for (size_t i = prefix.size(); i < us.Length / sizeof(char16_t); i++) {
        if (us.Buffer[i] == u'/')
            us.Buffer[i] = u'\\';
    }

    free(fni2);

    return true;
}

extern "C"
int __chdir(const char* path) {
    const auto& teb = *NtCurrentTeb();
    const auto& peb = *teb.ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    nt_status status;

    if (!path || !path[0]) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, len), sv);

    us.Length = us.MaximumLength = (uint16_t)(len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb.ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_TRAVERSE | SYNCHRONIZE, &oa, &iosb, nullptr, 0,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_ALERT,
                          nullptr, 0);
    free(buf);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        if (!get_dos_path(h, peb.ProcessParameters->CurrentDirectory.DosPath)) {
            NtClose(h);
            return -1;
        }

        if (peb.ProcessParameters->CurrentDirectory.Handle != 0)
            NtClose(peb.ProcessParameters->CurrentDirectory.Handle);

        peb.ProcessParameters->CurrentDirectory.Handle = h;
    }

    return 0;
}

extern "C"
char* __fullpath(char* abs_path, const char* rel_path, size_t size) {
    char tmp[PATH_MAX], canon[PATH_MAX];
    size_t canon_len = 1;

    if (!rel_path || rel_path[0] == 0) {
        if (!_getcwd(tmp, PATH_MAX))
            return nullptr;
    } else if (rel_path[0] == '/') {
        if (strlen(rel_path) >= PATH_MAX) {
            *__errno() = ERANGE;
            return nullptr;
        }

        strcpy(tmp, rel_path);
    } else {
        if (!_getcwd(tmp, PATH_MAX))
            return nullptr;

        auto cwd_len = strlen(tmp);

        if (cwd_len + 1 + strlen(rel_path) >= PATH_MAX) {
            *__errno() = ERANGE;
            return nullptr;
        }

        tmp[cwd_len] = '/';
        strcpy(tmp + cwd_len + 1, rel_path);
    }

    canon[0] = '/';
    canon[1] = 0;

    char* p = tmp;

    if (*p == '/')
        p++;

    while (*p) {
        while (*p == '/') {
            p++;
        }

        if (!*p)
            break;

        char* comp = p;
        while (*p && *p != '/') {
            p++;
        }

        auto comp_len = (size_t)(p - comp);

        if (comp_len == 2 && comp[0] == '.' && comp[1] == '.') {
            char* last = canon + canon_len - 1;

            while (last > canon && *last != '/') {
                last--;
            }

            if (last == canon)
                canon_len = 1;
            else
                canon_len = (size_t)(last - canon);

            canon[canon_len] = '\0';
        } else if (comp_len != 1 || comp[0] != '.') {
            size_t need = canon_len + comp_len + 1;

            if (canon_len > 1)
                need++;

            if (need > PATH_MAX) {
                *__errno() = ERANGE;
                return nullptr;
            }

            if (canon_len > 1) {
                canon[canon_len] = '/';
                canon_len++;
            }

            memcpy(canon + canon_len, comp, comp_len);
            canon_len += comp_len;
            canon[canon_len] = '\0';
        }
    }

    char* ret;

    if (abs_path) {
        if (canon_len + 1 > size) {
            *__errno() = ERANGE;
            return nullptr;
        }

        ret = abs_path;
    } else {
        ret = (char*)malloc(canon_len + 1);
        if (!ret) {
            *__errno() = ENOMEM;
            return nullptr;
        }
    }

    memcpy(ret, canon, canon_len + 1);

    return ret;
}
