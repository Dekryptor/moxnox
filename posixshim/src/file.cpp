#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

import nt;
import unicode;

#undef errno
import errno;

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

extern "C"
int lstat(const char* path, struct stat* statbuf) {
    // FIXME - symlinks

    return stat(path, statbuf);
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
