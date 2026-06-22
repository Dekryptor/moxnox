#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string_view>
#include <span>

import nt;
import unicode;
import unique_handle;

#undef errno
import errno;

using namespace std;

extern "C"
int mkdir(const char* path, mode_t /*mode*/) {
    return _mkdir(path);
}

extern "C"
ssize_t getline(char** lineptr, size_t* pn, FILE* stream) {
    if (!lineptr || !pn) {
        *_errno() = EINVAL;
        return -1;
    }

    auto& line = *lineptr;
    auto& n = *pn;
    size_t read = 0;

    if (!line || n == 0) {
        auto newptr = (char*)realloc(line, 128);
        if (!newptr) {
            *_errno() = ENOMEM;
            return -1;
        }

        line = newptr;
        n = 128;
    }

    auto add_char = [&read, &n, &line](char c) {
        if (read == n) {
            auto newptr = (char*)realloc(line, n * 2);
            if (!newptr) {
                *_errno() = ENOMEM;
                return false;
            }

            line = newptr;
            n *= 2;
        }

        line[read] = c;

        return true;
    };

    while (true) {
        auto c = fgetc(stream);

        if (c == EOF)
            break;

        if (!add_char((char)c))
            return -1;

        read++;

        if ((char)c == '\n')
            break;
    }

    if (read == 0)
        return -1;

    if (!add_char(0))
        return -1;

    return (ssize_t)read;
}

__time64_t win_to_unix_time(int64_t t) {
    return (__time64_t)((t - 116444736000000000) / 10000000);
}

extern "C"
int lstat(const char* path, struct stat* st) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    FILE_BASIC_INFORMATION fbi;
    FILE_STANDARD_INFORMATION fsi;
    HANDLE h;

    if (!path || path[0] == 0) {
        *_errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          FILE_OPEN_REPARSE_POINT, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    unique_handle f{h};

    status = NtQueryInformationFile(f.get(), &iosb, &fbi, sizeof(fbi),
                                    FILE_INFORMATION_CLASS::BasicInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    status = NtQueryInformationFile(f.get(), &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    memset(st, 0, sizeof(*st));

    if (fbi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = S_IFDIR | 0777;
    else if (fbi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        st->st_mode = S_IFLNK | 0777;
    else {
        st->st_mode = S_IFREG | 0444;

        if (!(fbi.FileAttributes & FILE_ATTRIBUTE_READONLY))
            st->st_mode |= 0222;
    }

    st->st_nlink = (short)fsi.NumberOfLinks;
    st->st_size = fsi.EndOfFile.QuadPart;
    st->st_atime = win_to_unix_time(fbi.LastAccessTime.QuadPart);
    st->st_mtime = win_to_unix_time(fbi.LastWriteTime.QuadPart);
    st->st_ctime = win_to_unix_time(fbi.ChangeTime.QuadPart);

    return 0;
}

extern "C"
int futimens(int fd, const timespec* times) {
    FILE_BASIC_INFORMATION fbi;
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER now;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1) {
        *_errno() = EBADF;
        return -1;
    }

    auto timespec_to_filetime = [](const struct timespec& ts) {
        return (int64_t)((uint64_t)116444736000000000 + ((uint64_t)ts.tv_sec * 10000000) + ((uint64_t)ts.tv_nsec / 100));
    };

    fbi.CreationTime.QuadPart = 0;
    fbi.ChangeTime.QuadPart = 0;
    fbi.FileAttributes = 0;

    NtQuerySystemTime(&now);

    if (!times) {
        fbi.LastAccessTime.QuadPart = now.QuadPart;
        fbi.LastWriteTime.QuadPart = now.QuadPart;
    } else {
        if (times[0].tv_nsec == UTIME_NOW)
            fbi.LastAccessTime.QuadPart = now.QuadPart;
        else if (times[0].tv_nsec == UTIME_OMIT)
            fbi.LastAccessTime.QuadPart = 0;
        else
            fbi.LastAccessTime.QuadPart = timespec_to_filetime(times[0]);

        if (times[1].tv_nsec == UTIME_NOW)
            fbi.LastWriteTime.QuadPart = now.QuadPart;
        else if (times[1].tv_nsec == UTIME_OMIT)
            fbi.LastWriteTime.QuadPart = 0;
        else
            fbi.LastWriteTime.QuadPart = timespec_to_filetime(times[1]);
    }

    auto status = NtSetInformationFile(h, &iosb, &fbi, sizeof(fbi),
                                       FILE_INFORMATION_CLASS::BasicInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int ftruncate(int fd, off_t length) {
    FILE_END_OF_FILE_INFORMATION eof;
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1) {
        *_errno() = EBADF;
        return -1;
    }

    eof.EndOfFile.QuadPart = length;

    auto status = NtSetInformationFile(h, &iosb, &eof, sizeof(eof),
                                       FILE_INFORMATION_CLASS::EndOfFileInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int pipe(int* pfds) {
    return _pipe(pfds, 0, 0);
}
