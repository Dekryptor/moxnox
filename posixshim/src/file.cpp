#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string_view>
#include <span>
#include <memory>
#include <mutex>

import nt;
import ntdll;
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
    FILE_INTERNAL_INFORMATION fii;
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

    status = NtQueryInformationFile(h, &iosb, &fii, sizeof(fii),
                                    FILE_INFORMATION_CLASS::InternalInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    memset(st, 0, sizeof(*st));

    // FIXME - inode number only 16 bits on mingw(???)

    st->st_ino = (_ino_t)fii.IndexNumber.QuadPart;

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

extern "C"
ssize_t readlink(const char* path, char* buf, size_t bufsiz) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    struct {
        REPARSE_DATA_BUFFER header;
        char16_t buffer[255];
    } rdb;

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

    status = NtFsControlFile(f.get(), 0, nullptr, nullptr, &iosb,
                             FSCTL_GET_REPARSE_POINT, nullptr, 0,
                             &rdb, sizeof(rdb));
    if (!success(status)) {
        if (status == nt_status::NOT_A_REPARSE_POINT)
            *_errno() = EINVAL;
        else
            *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (rdb.header.ReparseTag != IO_REPARSE_TAG_SYMLINK) {
        *_errno() = EINVAL;
        return -1;
    }

    auto u16sv = u16string_view(rdb.header.SymbolicLinkReparseBuffer.PathBuffer + (rdb.header.SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(char16_t)),
                                rdb.header.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(u16sv);

    utf16_to_utf8(span(buf, bufsiz), u16sv);

    return (ssize_t)min(u8len, bufsiz);
}

extern "C"
int symlink(const char* target, const char* linkpath) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle f;

    if (!linkpath || !linkpath[0]) {
        *_errno() = EINVAL;
        return -1;
    }

    if (!target || !target[0]) {
        *_errno() = EINVAL;
        return -1;
    }

    auto linkpathsv = string_view(linkpath);
    auto targetsv = string_view(target);

    auto u16len = utf8_to_utf16_len(linkpathsv);
    auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16, u16len), linkpathsv);

    // try to create symlink

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES | DELETE, &oa, &iosb, nullptr,
                          0, 0, FILE_CREATE, FILE_NON_DIRECTORY_FILE, nullptr, 0);

    free(u16);

    if (status == nt_status::OBJECT_NAME_COLLISION) {
        *_errno() = EEXIST;
        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    } else
        f.reset(h);

    auto u16targetlen = utf8_to_utf16_len(targetsv);

    size_t rdblen = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) + (u16targetlen * sizeof(char16_t));

    auto rdbbuf = malloc(rdblen);
    if (!rdbbuf) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        NtSetInformationFile(f.get(), &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);

        *_errno() = ENOMEM;
        return -1;
    }

    auto& rdb = *(REPARSE_DATA_BUFFER*)rdbbuf;

    rdb.ReparseTag = IO_REPARSE_TAG_SYMLINK;
    rdb.ReparseDataLength = (uint16_t)(rdblen - offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer));
    rdb.Reserved = 0;
    rdb.SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
    rdb.SymbolicLinkReparseBuffer.SubstituteNameLength = (uint16_t)(u16targetlen * sizeof(char16_t));
    rdb.SymbolicLinkReparseBuffer.PrintNameOffset = 0;
    rdb.SymbolicLinkReparseBuffer.PrintNameLength = 0;
    rdb.SymbolicLinkReparseBuffer.Flags = 0;

    utf8_to_utf16(span(rdb.SymbolicLinkReparseBuffer.PathBuffer, u16targetlen), targetsv);

    status = NtFsControlFile(f.get(), 0, nullptr, nullptr, &iosb,
                             FSCTL_SET_REPARSE_POINT, &rdb, (uint32_t)rdblen,
                             nullptr, 0);

    free(rdbbuf);

    if (!success(status)) {
        FILE_DISPOSITION_INFORMATION fdi;

        fdi.DeleteFile = true;

        NtSetInformationFile(f.get(), &iosb, &fdi, sizeof(fdi),
                             FILE_INFORMATION_CLASS::DispositionInformation);

        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
void flockfile(FILE* f) {
    _lock_file(f);
}

extern "C"
void funlockfile(FILE* f) {
    _unlock_file(f);
}

extern "C"
int getc_unlocked(FILE* f) {
    return _fgetc_nolock(f);
}

static bool get_dos_path(HANDLE h, UNICODE_STRING& us) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;

    status = NtQueryInformationFile(h, &iosb, &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        *_errno() = nt_status_to_errno(status);
        return false;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        *_errno() = ENOMEM;
        return false;
    }

    status = NtQueryInformationFile(h, &iosb, fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
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
int fchdir(int fd) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    HANDLE h2;
    nt_status status;

    auto h = (HANDLE)_get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    status = NtDuplicateObject(NtCurrentProcess(), h, NtCurrentProcess(), &h2,
                               0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        if (!get_dos_path(h, peb.ProcessParameters->CurrentDirectory.DosPath)) {
            NtClose(h2);
            return -1;
        }

        if (peb.ProcessParameters->CurrentDirectory.Handle != 0)
            NtClose(peb.ProcessParameters->CurrentDirectory.Handle);

        peb.ProcessParameters->CurrentDirectory.Handle = h2;
    }

    return 0;
}

extern "C"
int fchmod(int /*fd*/, mode_t /*mode*/) {
    // FIXME - actually do something

    return 0;
}
