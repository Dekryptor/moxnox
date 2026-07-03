module;

#include <wchar.h>
#include <string_view>
#include <span>
#include <optional>
#include <atomic>
#include <mutex>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <sys/utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

export module file;

import print;
import nt;
import unicode;
import ntdll;
import list;
import ucrtbase;
import kernelbase;
import unique_handle;
import time;
import moxnox_ioctls;
import format;
import exec;

#undef errno
import errno;

using namespace std;

struct _FILE {
    _FILE(HANDLE h, int fd);
    ~_FILE();

    HANDLE h;
    int fd;
    RTL_CRITICAL_SECTION cs;
    list_entry le;
    optional<unsigned char> ungetc_char;
    HANDLE proc = 0;
    bool error = false;
    bool eof = false;
};

struct fd {
    HANDLE h;
    bool cloexec;
};

// ntdll
extern "C" void (*mn_handle_exec_info)(exec_info* info) __declspec(dllimport);

static void handle_exec_info(exec_info* info);

static array<fd, 2048> fds;

static uint8_t _stdin[sizeof(_FILE)], _stdout[sizeof(_FILE)], _stderr[sizeof(_FILE)];
static RTL_CRITICAL_SECTION files_lock, fds_lock;
static list_entry files_list = { &files_list, &files_list };

static int umask_value = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

// FIXME - should be getting from sys/stat.h
extern "C" int mkdir(const char* path, mode_t mode);

extern "C" int _getpid();

extern "C" int* __errno();

// dir.cpp
extern "C" int __chdir(const char* path);
extern "C++" bool get_dos_path(HANDLE h, UNICODE_STRING& us);

extern "C++"
void* operator new(size_t count) {
    return malloc(count);
}

extern "C++"
void operator delete(void* ptr) noexcept {
    free(ptr);
}

extern "C++"
void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

extern "C"
FILE* ___acrt_iob_func(unsigned int idx) {
    switch (idx) {
        case 0:
            return reinterpret_cast<FILE*>(&_stdin);

        case 1:
            return reinterpret_cast<FILE*>(&_stdout);

        case 2:
            return reinterpret_cast<FILE*>(&_stderr);

        default:
            return nullptr;
    }
}

extern "C"
void __lock_file(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    RtlEnterCriticalSection(&f.cs);
}

extern "C"
void __unlock_file(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    RtlLeaveCriticalSection(&f.cs);
}

extern "C"
int __fputc_nolock(int c, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    IO_STATUS_BLOCK iosb;
    char s[1];

    // FIXME - buffering

    s[0] = (char)c;

    auto status = NtWriteFile(f.h, 0, nullptr, nullptr, &iosb, s, 1,
                              nullptr, nullptr);
    if (!success(status))
        return EOF;

    return (int)s[0];
}

extern "C"
int _fputc(int c, FILE* file) {
    __lock_file(file);
    auto ret = __fputc_nolock(c, file);
    __unlock_file(file);

    return ret;
}

_FILE::_FILE(HANDLE h, int fd) : h(h), fd(fd) {
    RtlInitializeCriticalSection(&cs);
}

_FILE::~_FILE() {
    RtlDeleteCriticalSection(&cs);

    RtlEnterCriticalSection(&fds_lock);
    fds[(size_t)fd].h = 0;
    RtlLeaveCriticalSection(&fds_lock);
}

export void init_files() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    RtlInitializeCriticalSection(&files_lock);
    RtlInitializeCriticalSection(&fds_lock);

    auto f1 = new (_stdin) _FILE(peb->ProcessParameters->StandardInput, 0);
    auto f2 = new (_stdout) _FILE(peb->ProcessParameters->StandardOutput, 1);
    auto f3 = new (_stderr) _FILE(peb->ProcessParameters->StandardError, 2);

    insert_tail_list(files_list, f1->le);
    insert_tail_list(files_list, f2->le);
    insert_tail_list(files_list, f3->le);

    fds[0].h = peb->ProcessParameters->StandardInput;
    fds[0].cloexec = false;
    fds[1].h = peb->ProcessParameters->StandardOutput;
    fds[1].cloexec = false;
    fds[2].h = peb->ProcessParameters->StandardError;
    fds[2].cloexec = false;

    mn_handle_exec_info = handle_exec_info;
}

static bool do_open(const char* pathname, const char* mode, HANDLE& h, int& fd,
                    bool for_delete, bool cloexec) {
    enum class mode_type {
        r,
        rplus,
        w,
        wplus,
        a,
        aplus,
    };

    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    enum mode_type type;
    ACCESS_MASK perms;
    unsigned int create_options;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;

    if (!mode || !pathname || pathname[0] == 0) {
        *__errno() = EINVAL;
        return false;
    }

    switch (mode[0]) {
        case 'r':
            if (mode[1] == '+') {
                type = mode_type::rplus;
                mode += 2;
            } else {
                type = mode_type::r;
                mode++;
            }
            break;

        case 'w':
            if (mode[1] == '+') {
                type = mode_type::wplus;
                mode += 2;
            } else {
                type = mode_type::w;
                mode++;
            }
            break;

        case 'a':
            if (mode[1] == '+') {
                type = mode_type::aplus;
                mode += 2;
            } else {
                type = mode_type::a;
                mode++;
            }
            break;

        default:
            *__errno() = EINVAL;
            return false;
    }

    // FIXME - implement MS' extensions?
    // FIXME - is "rb+" etc. valid?
    while (*mode != 0) {
        if (*mode != 't' && *mode != 'b') {
            *__errno() = EINVAL;
            return false;
        }

        mode++;
    }

    switch (type) {
        using enum mode_type;

        case r:
            perms = FILE_READ_DATA;
            create_options = FILE_OPEN;
            break;

        case rplus:
            perms = FILE_READ_DATA | FILE_WRITE_DATA;
            create_options = FILE_OPEN;
            break;

        case w:
            perms = FILE_WRITE_DATA;
            create_options = FILE_OVERWRITE_IF;
            break;

        case wplus:
            perms = FILE_READ_DATA | FILE_WRITE_DATA;
            create_options = FILE_OVERWRITE_IF;
            break;

        case a:
            perms = FILE_APPEND_DATA;
            create_options = FILE_OPEN_IF;
            break;

        case aplus:
            perms = FILE_READ_DATA | FILE_APPEND_DATA;
            create_options = FILE_OPEN_IF;
            break;
    }

    if (for_delete)
        perms |= DELETE;

    auto sv = string_view(pathname);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *__errno() = ENOMEM;
        return false;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    auto status = NtCreateFile(&h, perms, &oa, &iosb, nullptr,
                               FILE_ATTRIBUTE_NORMAL, 0, create_options,
                               0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return false;
    }

    free(u16pathname);

    optional<int> optfd;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = h;
            fds[i].cloexec = cloexec;
            optfd = i;
            break;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    if (!optfd.has_value()) {
        NtClose(h);
        *__errno() = EMFILE;
        return false;
    }

    fd = *optfd;

    return true;
}

extern "C"
FILE* _fopen(const char* pathname, const char* mode) {
    HANDLE h;
    int fd;

    // FIXME - cloexec if "e" is in mode

    if (!do_open(pathname, mode, h, fd, false, false))
        return nullptr;

    auto f = new _FILE(h, fd);
    if (!f) {
        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)fd].h = 0;
        RtlLeaveCriticalSection(&fds_lock);

        NtClose(h);

        *__errno() = ENOMEM;
        return nullptr;
    }

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
size_t __fread_nolock(void* ptr, size_t size, size_t n, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    IO_STATUS_BLOCK iosb;
    size_t bytes = size * n, ungetc_bytes = 0;

    // FIXME - buffering

    if (bytes == 0)
        return 0;

    if (f.ungetc_char.has_value()) {
        *(uint8_t*)ptr = *f.ungetc_char;
        f.ungetc_char = nullopt;

        ptr = (uint8_t*)ptr + 1;
        bytes--;
        ungetc_bytes = 1;
    }

    auto status = NtReadFile(f.h, 0, nullptr, nullptr, &iosb, ptr,
                             (uint32_t)bytes, nullptr, nullptr);
    if (status == nt_status::END_OF_FILE) {
        f.eof = true;
        return ungetc_bytes;
    } else if (!success(status)) {
        f.error = true;
        return ungetc_bytes;
    }

    if (iosb.Information < bytes)
        f.eof = true;

    return iosb.Information + ungetc_bytes;
}

extern "C"
size_t _fread(void* ptr, size_t size, size_t n, FILE* file) {
    __lock_file(file);
    auto ret = __fread_nolock(ptr, size, n, file);
    __unlock_file(file);

    return ret;
}

extern "C"
size_t __fwrite_nolock(const void* ptr, size_t size, size_t n, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    IO_STATUS_BLOCK iosb;

    // FIXME - buffering

    auto status = NtWriteFile(f.h, 0, nullptr, nullptr, &iosb, ptr,
                              (uint32_t)(size * n), nullptr, nullptr);
    if (!success(status)) {
        f.error = true;
        return 0;
    }

    return iosb.Information;
}

extern "C"
size_t _fwrite(const void* ptr, size_t size, size_t n, FILE* file) {
    __lock_file(file);
    auto ret = __fwrite_nolock(ptr, size, n, file);
    __unlock_file(file);

    return ret;
}

extern "C"
int _fclose(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    // FIXME - do we need to make sure this isn't stdin, stdout, or stderr?

    {
        RtlEnterCriticalSection(&files_lock);
        remove_entry_list(f.le);
        RtlLeaveCriticalSection(&files_lock);
    }

    NtClose(f.h);
    delete &f;

    return 0;
}

extern "C"
int _fputs(const char* s, FILE* file) {
    auto len = strlen(s);

    __lock_file(file);
    auto ret = __fwrite_nolock(s, 1, len, file);
    __unlock_file(file);

    if (ret != len)
        return EOF;
    else
        return 0;
}

extern "C"
int puts(const char* s) {
    auto len = strlen(s);
    auto* f = reinterpret_cast<FILE*>(_stdout);
    const char c = '\n';

    __lock_file(f);
    auto ret = __fwrite_nolock(s, 1, len, f);

    if (ret != len) {
        __unlock_file(f);
        return EOF;
    }

    ret = __fwrite_nolock(&c, 1, 1, f);
    __unlock_file(f);

    if (ret != len)
        return EOF;
    else
        return 0;
}

extern "C"
int __fgetc_nolock(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    unsigned char c;

    if (f.ungetc_char.has_value()) {
        c = *f.ungetc_char;
        f.ungetc_char = nullopt;
        return (int)c;
    }

    // FIXME - buffering

    auto ret = __fread_nolock(&c, 1, 1, file);
    if (ret == 0)
        return EOF;

    return (int)c;
}

extern "C"
int _fgetc(FILE* file) {
    __lock_file(file);
    auto ret = __fgetc_nolock(file);
    __unlock_file(file);

    return ret;
}

extern "C"
int __ungetc_nolock(int c, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    if (f.ungetc_char.has_value())
        return EOF;

    f.ungetc_char = (unsigned char)c;

    f.error = false;
    f.eof = false;

    return c;
}

extern "C"
int _ungetc(int c, FILE* file) {
    __lock_file(file);
    auto ret = __ungetc_nolock(c, file);
    __unlock_file(file);

    return ret;
}

extern "C"
char* _fgets(char* s, int size, FILE* file) {
    int copied = 0;

    __lock_file(file);

    for (int i = 0; i < size - 1; i++) {
        auto c = __fgetc_nolock(file);
        if (c == EOF)
            break;

        s[i] = (char)c;
        copied++;

        if (c == '\n')
            break;
    }

    __unlock_file(file);

    if (copied == 0)
        return nullptr;

    s[copied] = 0;

    return s;
}

extern "C"
int64_t __ftelli64_nolock(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;

    auto status = NtQueryInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                         FILE_INFORMATION_CLASS::PositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    auto ret = fpi.CurrentByteOffset.QuadPart;

    if (f.ungetc_char.has_value())
        ret--;

    return ret;
}

extern "C"
int64_t __ftelli64(FILE* file) {
    __lock_file(file);
    auto ret = __ftelli64_nolock(file);
    __unlock_file(file);

    return ret;
}

extern "C"
int32_t __ftell_nolock(FILE* file) {
    return (int32_t)__ftelli64_nolock(file);
}

extern "C"
int32_t _ftell(FILE* file) {
    __lock_file(file);
    auto ret = __ftelli64_nolock(file);
    __unlock_file(file);

    return (int32_t)ret;
}

extern "C"
int __fseeki64_nolock(FILE* file, int64_t offset, int whence) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    int64_t start;
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;
    nt_status status;

    switch (whence) {
        case SEEK_SET:
            start = 0;
            break;

        case SEEK_CUR:
            start = __ftelli64_nolock(file);

            if (start == -1)
                return -1;
            break;

        case SEEK_END: {
            FILE_STANDARD_INFORMATION fsi;

            status = NtQueryInformationFile(f.h, &iosb, &fsi, sizeof(fsi),
                                            FILE_INFORMATION_CLASS::StandardInformation);
            if (!success(status)) {
                *__errno() = nt_status_to_errno(status);
                return -1;
            }

            start = fsi.EndOfFile.QuadPart;
            break;
        }

        default:
            *__errno() = EINVAL;
            return -1;
    }

    fpi.CurrentByteOffset.QuadPart = start + offset;

    status = NtSetInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                  FILE_INFORMATION_CLASS::PositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    f.ungetc_char = nullopt;
    f.eof = false;

    return 0;
}

extern "C"
int __fseeki64(FILE* file, int64_t offset, int whence) {
    __lock_file(file);
    auto ret = __fseeki64_nolock(file, offset, whence);
    __unlock_file(file);

    return ret;
}

extern "C"
int __fseek_nolock(FILE* file, int32_t offset, int whence) {
    return __fseeki64_nolock(file, offset, whence);
}

extern "C"
int _fseek(FILE* file, int32_t offset, int whence) {
    __lock_file(file);
    auto ret = __fseeki64_nolock(file, offset, whence);
    __unlock_file(file);

    return ret;
}

extern "C"
int _remove(const char* pathname) {
    if (!pathname || pathname[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(pathname);

    auto u16len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), sv);

    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    auto status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0,
                               FILE_OPEN, 0, nullptr, 0);
    if (!success(status)) {
        free(buf);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(buf);

    fdi.DeleteFile = true;

    status = NtSetInformationFile(h, &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        NtClose(h);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    NtClose(h);

    return 0;
}

export extern "C"
int _fflush(FILE* file) {
    if (!file) {
        int ret = 0;

        RtlEnterCriticalSection(&files_lock);

        for (auto le = files_list.next; le != &files_list; le = le->next) {
            auto& f = containing_record<_FILE>(le);

            if (auto ret2 = _fflush(reinterpret_cast<FILE*>(&f)); ret2 != 0)
                ret = ret2;
        }

        RtlLeaveCriticalSection(&files_lock);

        return ret;
    }

    // FIXME - flush

    return 0;
}

extern "C"
int atexit(_PVFV) {
    // stub - __mingw_vfprintf uses this through gdtoa

    return 0;
}

export extern "C"
int __stdio_common_vfprintf(uint64_t /*options*/, FILE* file,
                            const char* format, _locale_t /*locale*/,
                            va_list valist) {
    // FIXME - options
    // FIXME - locale

    return __mingw_vfprintf(file, format, valist);
}

extern "C"
int __stdio_common_vsprintf(uint64_t /*options*/, char* str, size_t len,
                            const char* format, _locale_t /*locale*/,
                            va_list valist) {
    // FIXME - options
    // FIXME - locale

    if (len == (size_t)-1)
        return __mingw_vsprintf(str, format, valist);
    else
        return __mingw_vsnprintf(str, len, format, valist);
}

extern "C"
int __stdio_common_vfwprintf(uint64_t /*options*/, FILE* file,
                             const wchar_t* format, _locale_t /*locale*/,
                             va_list valist) {
    // FIXME - options
    // FIXME - locale

    return __mingw_vfwprintf(file, format, valist);
}

extern "C"
int setvbuf(FILE* /*stream*/, char* /*buffer*/, int /*mode*/, size_t /*size*/) {
    // FIXME - change buffering mode of stream?

    return 0;
}

extern "C"
void setbuf(FILE* file, char* buf) {
    setvbuf(file, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

static int do_fstat(HANDLE h, struct ::_stat64* st) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_BASIC_INFORMATION fbi;
    FILE_STANDARD_INFORMATION fsi;
    FILE_INTERNAL_INFORMATION fii;

    status = NtQueryInformationFile(h, &iosb, &fbi, sizeof(fbi),
                                    FILE_INFORMATION_CLASS::BasicInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    status = NtQueryInformationFile(h, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    status = NtQueryInformationFile(h, &iosb, &fii, sizeof(fii),
                                    FILE_INFORMATION_CLASS::InternalInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    memset(st, 0, sizeof(*st));

    // FIXME - inode number only 16 bits on mingw(???)

    st->st_ino = (_ino_t)fii.IndexNumber.QuadPart;

    if (fbi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = S_IFDIR | 0777;
    else if (fbi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        st->st_mode = S_IFLNK | 0777;
    else if (fbi.FileAttributes & FILE_ATTRIBUTE_DEVICE)
        st->st_mode = S_IFCHR | 0666; // FIXME - better than nothing
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
int ___stat64(const char* path, struct _stat64* st) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    nt_status status;

    if (!path || path[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);

    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *__errno() = ENOMEM;
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
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, 0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    unique_handle f{h};

    return do_fstat(f.get(), st);
}

extern "C"
int __stat64i32(const char* path, struct _stat64i32* st) {
    struct _stat64 st2;

    auto ret = ___stat64(path, &st2);
    if (ret)
        return ret;

    st->st_dev = st2.st_dev;
    st->st_ino = st2.st_ino;
    st->st_mode = st2.st_mode;
    st->st_nlink = st2.st_nlink;
    st->st_uid = st2.st_uid;
    st->st_gid = st2.st_gid;
    st->st_rdev = st2.st_rdev;
    st->st_size = (int32_t)st2.st_size;
    st->st_atime = st2.st_atime;
    st->st_mtime = st2.st_mtime;
    st->st_ctime = st2.st_ctime;

    return 0;
}

extern "C"
int __stat32i64(const char* path, struct _stat32i64* st) {
    struct _stat64 st2;

    auto ret = ___stat64(path, &st2);
    if (ret)
        return ret;

    st->st_dev = st2.st_dev;
    st->st_ino = st2.st_ino;
    st->st_mode = st2.st_mode;
    st->st_nlink = st2.st_nlink;
    st->st_uid = st2.st_uid;
    st->st_gid = st2.st_gid;
    st->st_rdev = st2.st_rdev;
    st->st_size = st2.st_size;
    st->st_atime = (__time32_t)st2.st_atime;
    st->st_mtime = (__time32_t)st2.st_mtime;
    st->st_ctime = (__time32_t)st2.st_ctime;

    return 0;
}

extern "C"
int __stat32(const char* path, struct _stat32* st) {
    struct _stat64 st2;

    auto ret = ___stat64(path, &st2);
    if (ret)
        return ret;

    st->st_dev = st2.st_dev;
    st->st_ino = st2.st_ino;
    st->st_mode = st2.st_mode;
    st->st_nlink = st2.st_nlink;
    st->st_uid = st2.st_uid;
    st->st_gid = st2.st_gid;
    st->st_rdev = st2.st_rdev;
    st->st_size = (int32_t)st2.st_size;
    st->st_atime = (__time32_t)st2.st_atime;
    st->st_mtime = (__time32_t)st2.st_mtime;
    st->st_ctime = (__time32_t)st2.st_ctime;

    return 0;
}

extern "C"
int __fstat64(int fd, struct _stat64* st) {
    HANDLE h;

    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EINVAL;
        return -1;
    }

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd].h;
    RtlLeaveCriticalSection(&fds_lock);

    if (!h) {
        *__errno() = EINVAL;
        return -1;
    }

    return do_fstat(h, st);
}

extern "C"
int __fstat64i32(int fd, struct _stat64i32* st) {
    struct _stat64 st2;

    auto ret = __fstat64(fd, &st2);
    if (ret)
        return ret;

    st->st_dev = st2.st_dev;
    st->st_ino = st2.st_ino;
    st->st_mode = st2.st_mode;
    st->st_nlink = st2.st_nlink;
    st->st_uid = st2.st_uid;
    st->st_gid = st2.st_gid;
    st->st_rdev = st2.st_rdev;
    st->st_size = (int32_t)st2.st_size;
    st->st_atime = st2.st_atime;
    st->st_mtime = st2.st_mtime;
    st->st_ctime = st2.st_ctime;

    return 0;
}

extern "C"
int __fstat32i64(int fd, struct _stat32i64* st) {
    struct _stat64 st2;

    auto ret = __fstat64(fd, &st2);
    if (ret)
        return ret;

    st->st_dev = st2.st_dev;
    st->st_ino = st2.st_ino;
    st->st_mode = st2.st_mode;
    st->st_nlink = st2.st_nlink;
    st->st_uid = st2.st_uid;
    st->st_gid = st2.st_gid;
    st->st_rdev = st2.st_rdev;
    st->st_size = st2.st_size;
    st->st_atime = (__time32_t)st2.st_atime;
    st->st_mtime = (__time32_t)st2.st_mtime;
    st->st_ctime = (__time32_t)st2.st_ctime;

    return 0;
}

extern "C"
int __fstat32(int fd, struct _stat32* st) {
    struct _stat64 st2;

    auto ret = __fstat64(fd, &st2);
    if (ret)
        return ret;

    st->st_dev = st2.st_dev;
    st->st_ino = st2.st_ino;
    st->st_mode = st2.st_mode;
    st->st_nlink = st2.st_nlink;
    st->st_uid = st2.st_uid;
    st->st_gid = st2.st_gid;
    st->st_rdev = st2.st_rdev;
    st->st_size = (int32_t)st2.st_size;
    st->st_atime = (__time32_t)st2.st_atime;
    st->st_mtime = (__time32_t)st2.st_mtime;
    st->st_ctime = (__time32_t)st2.st_ctime;

    return 0;
}

extern "C"
int ferror(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    return f.error ? 1 : 0;
}

extern "C"
int feof(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    return f.eof ? 1 : 0;
}

static int open2(const char* path, int flags, int pmode) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    ACCESS_MASK access = FILE_READ_ATTRIBUTES;
    uint32_t file_attributes, create_disposition, create_options;
    nt_status status;

    if (flags & _O_TEMPORARY) {
        print_stderr("FIXME - _open with _O_TEMPORARY\n");
        flags &= ~_O_TEMPORARY;
    }

    if (flags & _O_NOINHERIT) {
        print_stderr("FIXME - _open with _O_NOINHERIT\n");
        flags &= ~_O_NOINHERIT;
    }

    if (flags & _O_WTEXT) {
        print_stderr("FIXME - _open with _O_WTEXT\n");
        flags &= ~_O_WTEXT;
    }

    if (flags & _O_U16TEXT) {
        print_stderr("FIXME - _open with _O_U16TEXT\n");
        flags &= ~_O_U16TEXT;
    }

    if (flags & _O_U8TEXT) {
        print_stderr("FIXME - _open with _O_U8TEXT\n");
        flags &= ~_O_U8TEXT;
    }

    if (flags & _O_U8TEXT) {
        print_stderr("FIXME - _open with _O_U8TEXT\n");
        flags &= ~_O_U8TEXT;
    }

    // FIXME - do we need to treat _O_TEXT and _O_BINARY differently?
    // FIXME - _O_RANDOM
    // FIXME - _O_SEQUENTIAL
    // FIXME - _O_SHORT_LIVED?

    if (flags & _O_WRONLY)
        access |= FILE_GENERIC_WRITE;
    else if (flags & _O_RDWR)
        access |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
    else
        access |= FILE_GENERIC_READ;

    if (flags & (_O_WRONLY | _O_RDWR)) {
        if (flags & _O_APPEND)
            access &= ~FILE_WRITE_DATA;
        else
            access &= ~FILE_APPEND_DATA;
    }

    if (flags & _O_CREAT) {
        if (flags & _O_EXCL)
            create_disposition = FILE_CREATE;
        else if (flags & _O_TRUNC)
            create_disposition = FILE_OVERWRITE_IF;
        else
            create_disposition = FILE_OPEN_IF;
    } else {
        if (flags & _O_TRUNC)
            create_disposition = FILE_OVERWRITE;
        else
            create_disposition = FILE_OPEN;
    }

    create_options = 0;

    if (flags & O_NOFOLLOW)
        create_options = FILE_OPEN_REPARSE_POINT;

    if (!(pmode & _S_IWRITE))
        file_attributes = FILE_ATTRIBUTE_READONLY;
    else
        file_attributes = FILE_ATTRIBUTE_NORMAL;

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, access, &oa, &iosb, nullptr, file_attributes,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, create_disposition,
                          create_options, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    if (flags & O_NOFOLLOW) {
        // check if reparse point

        FILE_BASIC_INFORMATION fbi;

        status = NtQueryInformationFile(h, &iosb, &fbi, sizeof(fbi),
                                        FILE_INFORMATION_CLASS::BasicInformation);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            NtClose(h);
            return -1;
        }

        if (fbi.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            *__errno() = ELOOP;
            NtClose(h);
            return -1;
        }
    }

    if (flags & O_NONBLOCK) {
        bool nonblock = true;

        // FIXME - behaviour is different for FIFOs (means open itself should not block)

        auto status = NtSetInformationFile(h, &iosb, &nonblock, sizeof(nonblock),
                                        FILE_INFORMATION_CLASS::MoxnoxPosixNonBlock);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            NtClose(h);
            return -1;
        }
    }

    optional<int> fd;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = h;
            fds[i].cloexec = !!(flags & O_CLOEXEC);
            fd = i;
            break;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    if (!fd.has_value()) {
        NtClose(h);
        *__errno() = ENFILE;
        return -1;
    }

    return *fd;
}

extern "C"
int __open(const char* path, int flags, ...) {
    va_list ap;

    if (flags & _O_CREAT) {
        int pmode;

        va_start(ap, flags);
        pmode = va_arg(ap, int);
        va_end(ap);

        return open2(path, flags, pmode);
    } else
        return open2(path, flags, 0);
}

static int sopen2(const char* path, int oflags, int /*shflags*/, int pmode) {
    // FIXME - shflags

    return open2(path, oflags, pmode);
}

extern "C"
int __sopen(const char* path, int oflags, int shflags, ...) {
    va_list ap;

    if (oflags & _O_CREAT) {
        int pmode;

        va_start(ap, shflags);
        pmode = va_arg(ap, int);
        va_end(ap);

        return sopen2(path, oflags, shflags, pmode);
    } else
        return sopen2(path, oflags, shflags, 0);
}

extern "C"
FILE* __fdopen(int fd, const char* /*mode*/) {
    HANDLE h;

    // FIXME - do something or other with mode (read, write, append?)

    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EINVAL;
        return nullptr;
    }

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd].h;
    RtlLeaveCriticalSection(&fds_lock);

    if (!h) {
        *__errno() = EINVAL;
        return nullptr;
    }

    auto f = new _FILE(h, fd);
    if (!f) {
        *__errno() = ENOMEM;
        return nullptr;
    }

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
int __fileno(FILE* file) {
    if (!file) {
        *__errno() = EINVAL;
        return -1;
    }

    auto& f = *reinterpret_cast<_FILE*>(file);

    return f.fd;
}

extern "C"
int __chmod(const char* /*path*/, int /*flags*/) {
    // FIXME - do something (at least check path is valid)

    return 0;
}

extern "C"
int __utime64(const char* /*path*/, struct __utimbuf64* /*times*/) {
    // FIXME - implement

    return 0;
}

extern "C"
int __utime32(const char* path, struct __utimbuf32* times) {
    struct __utimbuf64 t;

    t.actime = times->actime;
    t.modtime = times->modtime;

    return __utime64(path, &t);
}

extern "C"
int getchar() {
    return _fgetc((FILE*)&_stdin);
}

extern "C"
intptr_t __get_osfhandle(int fd) {
    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    HANDLE h;

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd].h;
    RtlLeaveCriticalSection(&fds_lock);

    if (h == 0) {
        *__errno() = EBADF;
        return -1;
    }

    return (intptr_t)h;
}

extern "C"
int __open_osfhandle(intptr_t osfhandle, int /*flags*/) {
    optional<int> fd;

    // FIXME - flags?

    // FIXME - record last fd used somewhere, before we resort to searching the whole array?

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = (HANDLE)osfhandle;
            fds[i].cloexec = false;
            fd = i;
            break;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    if (!fd.has_value()) {
        *__errno() = ENFILE;
        return -1;
    }

    return *fd;
}

extern "C"
int __isatty(int fd) {
    IO_STATUS_BLOCK iosb;

    auto h = __get_osfhandle(fd);
    if (h == -1)
        return 0; // FIXME - is this right?

    auto status = NtDeviceIoControlFile((HANDLE)h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_ISATTY, nullptr, 0,
                                        nullptr, 0);
    if (!success(status))
        return 0;

    return 1;
}

extern "C"
int __write(int fd, const void* buffer, unsigned int count) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)__get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    auto status = NtWriteFile(h, 0, nullptr, nullptr, &iosb, buffer,
                              count, nullptr, nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return (int)iosb.Information;
}

extern "C"
int __read(int fd, void* buffer, size_t count) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)__get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    auto status = NtReadFile(h, 0, nullptr, nullptr, &iosb, buffer,
                             (uint32_t)count, nullptr, nullptr);
    if (status == nt_status::END_OF_FILE || status == nt_status::PIPE_DISCONNECTED)
        return 0;
    else if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return (int)iosb.Information;
}

extern "C"
int __access(const char* path, int /*mode*/) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    nt_status status;

    if (!path || path[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16pathname, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16pathname;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    // FIXME - translate mode into access mask
    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    return 0;
}

extern "C"
int putc(int c, FILE* file) {
    auto c2 = (char)c;

    __lock_file(file);
    auto ret = __fwrite_nolock(&c2, 1, 1, file);
    __unlock_file(file);

    if (ret != 1)
        return EOF;
    else
        return 0;
}

extern "C"
int putchar(int c) {
    return putc(c, reinterpret_cast<FILE*>(_stdout));
}

extern "C"
int __setmode(int /*fd*/, int /*mode*/) {
    // FIXME - actually do this?

    return 0;
}

extern "C"
int __close(int fd) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    HANDLE h = 0;

    RtlEnterCriticalSection(&fds_lock);
    swap(fds[(size_t)fd].h, h);
    RtlLeaveCriticalSection(&fds_lock);

    if (h == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    NtClose(h);

    // FIXME - should be HANDLE to /dev/null maybe?
    if (fd == 0)
        peb.ProcessParameters->StandardInput = 0;
    else if (fd == 1)
        peb.ProcessParameters->StandardOutput = 0;
    else if (fd == 2)
        peb.ProcessParameters->StandardError = 0;

    return 0;
}

extern "C"
int __mkdir(const char* path) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE file;
    IO_STATUS_BLOCK iosb;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    // FIXME - mode?

    auto pathsv = string_view(path);

    while (pathsv.size() > 1 && (pathsv.back() == '/' || pathsv.back() == '\\')) {
        pathsv = string_view(pathsv.data(), pathsv.size() - 1);
    }

    if (pathsv.empty()) {
        *__errno() = EINVAL;
        return -1;
    }

    auto u16len = utf8_to_utf16_len(pathsv);
    auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));

    if (!u16) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16, u16len), pathsv);

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
        *__errno() = EEXIST;
        return -1;
    } else if (status == nt_status::OBJECT_PATH_NOT_FOUND) {
        *__errno() = ENOENT;
        return -1;
    } else if (!success(status)) {
        *__errno() = EINVAL;
        return -1;
    }

    NtClose(file);

    return 0;
}

extern "C"
void clearerr(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    f.error = false;
    f.eof = false;
}

extern "C"
int rename(const char* oldpath, const char* newpath) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE h;
    unique_handle file;

    if (!oldpath || !newpath || oldpath[0] == 0 || newpath[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto oldsv = string_view(oldpath);
    auto u16len = utf8_to_utf16_len(oldsv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), oldsv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0, FILE_OPEN,
                          0, nullptr, 0);

    free(buf);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    auto newsv = string_view(newpath);
    u16len = utf8_to_utf16_len(newsv);

    size_t frilen = offsetof(FILE_RENAME_INFORMATION, FileName) + (u16len * sizeof(char16_t));

    auto fri = (FILE_RENAME_INFORMATION*)malloc(frilen);
    if (!fri) {
        *__errno() = ENOMEM;
        return -1;
    }

    fri->ReplaceIfExists = true; // FIXME? --interactive option
    fri->RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;
    fri->FileNameLength = (uint32_t)(u16len * sizeof(char16_t));
    utf8_to_utf16(span(fri->FileName, fri->FileNameLength / sizeof(char16_t)),
                  newsv);

    status = NtSetInformationFile(file.get(), &iosb, fri, (uint32_t)frilen,
                                  FILE_INFORMATION_CLASS::RenameInformation);

    free(fri);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int __unlink(const char* path) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h;
    unique_handle file;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!path || path[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_OPEN_REPARSE_POINT,
                          nullptr, 0);

    free(buf);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file.get(), &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int __dup2(int oldfd, int newfd) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;

    if (oldfd < 0 || (size_t)oldfd >= fds.size() || newfd < 0 || (size_t)newfd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    RtlEnterCriticalSection(&fds_lock);

    auto oldh = fds[(size_t)oldfd].h;

    if (!oldh) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = EBADF;
        return -1;
    }

    if (oldfd == newfd) {
        RtlLeaveCriticalSection(&fds_lock);
        return newfd;
    }

    HANDLE newh;

    auto status = NtDuplicateObject(NtCurrentProcess(), oldh, NtCurrentProcess(),
                                    &newh, 0, 0,
                                    DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status)) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    if (fds[(size_t)newfd].h != 0)
        NtClose(fds[(size_t)newfd].h);

    fds[(size_t)newfd].h = newh;
    fds[(size_t)newfd].cloexec = false;

    RtlLeaveCriticalSection(&fds_lock);

    if (newfd == 0) {
        peb.ProcessParameters->StandardInput = newh;
        reinterpret_cast<_FILE*>(&_stdin)->h = newh;
    } else if (newfd == 1) {
        peb.ProcessParameters->StandardOutput = newh;
        reinterpret_cast<_FILE*>(&_stdout)->h = newh;
    } else if (newfd == 2) {
        peb.ProcessParameters->StandardError = newh;
        reinterpret_cast<_FILE*>(&_stderr)->h = newh;
    }

    return newfd;
}

extern "C"
int __dup(int oldfd) {
    if (oldfd < 0 || (size_t)oldfd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    RtlEnterCriticalSection(&fds_lock);

    auto oldh = fds[(size_t)oldfd].h;

    if (!oldh) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = EBADF;
        return -1;
    }

    HANDLE newh;

    auto status = NtDuplicateObject(NtCurrentProcess(), oldh, NtCurrentProcess(),
                                    &newh, 0, 0,
                                    DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
    if (!success(status)) {
        RtlLeaveCriticalSection(&fds_lock);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h == 0) {
            fds[i].h = newh;
            fds[i].cloexec = false;
            RtlLeaveCriticalSection(&fds_lock);
            return (int)i;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    NtClose(newh);

    *__errno() = EMFILE;

    return -1;
}

extern "C"
int64_t _lseeki64(int fd, int64_t offset, int whence) {
    int64_t start;
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;
    nt_status status;

    auto h = (HANDLE)__get_osfhandle(fd);
    if (h == (HANDLE)-1)
        return -1;

    switch (whence) {
        case SEEK_SET:
            start = 0;
            break;

        case SEEK_CUR:
            status = NtQueryInformationFile(h, &iosb, &fpi, sizeof(fpi),
                                            FILE_INFORMATION_CLASS::PositionInformation);
            if (!success(status)) {
                *__errno() = nt_status_to_errno(status);
                return -1;
            }

            start = fpi.CurrentByteOffset.QuadPart;
            break;

        case SEEK_END: {
            FILE_STANDARD_INFORMATION fsi;

            status = NtQueryInformationFile(h, &iosb, &fsi, sizeof(fsi),
                                            FILE_INFORMATION_CLASS::StandardInformation);
            if (!success(status)) {
                *__errno() = nt_status_to_errno(status);
                return -1;
            }

            start = fsi.EndOfFile.QuadPart;
            break;
        }

        default:
            *__errno() = EINVAL;
            return -1;
    }

    fpi.CurrentByteOffset.QuadPart = start + offset;

    status = NtSetInformationFile(h, &iosb, &fpi, sizeof(fpi),
                                  FILE_INFORMATION_CLASS::PositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return start + offset;
}

extern "C"
long __lseek(int fd, long offset, int whence) {
    return (long)_lseeki64(fd, (int64_t)offset, whence);
}

extern "C"
void rewind(FILE* file) {
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;
    nt_status status;

    __lock_file(file);

    auto& f = *reinterpret_cast<_FILE*>(file);

    fpi.CurrentByteOffset.QuadPart = 0;

    status = NtSetInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                  FILE_INFORMATION_CLASS::PositionInformation);
    if (!success(status)) {
        __unlock_file(file);
        return;
    }

    f.ungetc_char = nullopt;
    f.error = false;
    f.eof = false;

    __unlock_file(file);
}

extern "C"
int __umask(int pmode) {
    // FIXME - actually use this when creating files or directories

    swap(pmode, umask_value);

    return pmode;
}

extern "C"
int __rmdir(const char* path) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE h;
    unique_handle file;
    IO_STATUS_BLOCK iosb;
    FILE_DISPOSITION_INFORMATION fdi;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!path || path[0] == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    auto sv = string_view(path);
    auto u16len = utf8_to_utf16_len(sv);

    auto buf = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!buf) {
        *__errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(buf, u16len), sv);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = (char16_t*)buf;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, DELETE, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);

    free(buf);

    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    file.reset(h);

    // FIXME - should we be asking for POSIX-style deletes?

    fdi.DeleteFile = true;

    status = NtSetInformationFile(file.get(), &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
int __creat(const char* path, int mode) {
    // FIXME - mode should actually be mode_t rather than int

    return __open(path, _O_CREAT | _O_WRONLY | _O_TRUNC, mode);
}

extern "C"
FILE* freopen(const char* path, const char* mode, FILE* file) {
    if (!file || !mode) {
        *__errno() = EINVAL;
        return nullptr;
    }

    auto& f = *reinterpret_cast<_FILE*>(file);
    HANDLE h;
    int fd;

    if (!path)
        return file; // FIXME - should also be changing mode

    // FIXME - cloexec if "e" flag set
    if (!do_open(path, mode, h, fd, false, false))
        return nullptr;

    __lock_file(file);

    RtlEnterCriticalSection(&fds_lock);
    fds[(size_t)f.fd].h = 0;
    RtlLeaveCriticalSection(&fds_lock);

    NtClose(f.h);

    f.h = h;
    f.fd = fd;
    f.ungetc_char.reset();
    f.error = false;
    f.eof = false;

    __unlock_file(file);

    return file;
}

extern "C"
FILE* tmpfile() {
    nt_status status;
    FILE_DISPOSITION_INFORMATION fdi;
    IO_STATUS_BLOCK iosb;
    static atomic<unsigned int> counter = 1;
    char path[PATH_MAX];
    span sp(path, sizeof(path));
    HANDLE h;
    int fd;

    // create file

    sp = format(sp, "/tmp/tmpfile_{}_{}", _getpid(), counter.fetch_add(1));
    sp[0] = 0;

    if (!do_open(path, "w+", h, fd, true, false))
        return nullptr;

    // unlink file

    fdi.DeleteFile = true;

    status = NtSetInformationFile(h, &iosb, &fdi, sizeof(fdi),
                                  FILE_INFORMATION_CLASS::DispositionInformation);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);

        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)fd].h = 0;
        RtlLeaveCriticalSection(&fds_lock);

        NtClose(h);

        return nullptr;
    }

    // create FILE

    auto f = new _FILE(h, fd);
    if (!f) {
        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)fd].h = 0;
        RtlLeaveCriticalSection(&fds_lock);

        NtClose(h);

        *__errno() = ENOMEM;
        return nullptr;
    }

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
int __pipe(int* pfds, unsigned int /*psize*/, int /*textmode*/) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    HANDLE server_handle, client_handle;
    nt_status status;
    char16_t pipename[50];
    optional<int> server_fd, client_fd;

    static atomic<unsigned int> counter = 1;

    // FIXME - psize?
    // FIXME - textmode?

    // FIXME - SIGPIPE and EPIPE, etc.

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    {
        char s[50];
        span sp(s, sizeof(s));

        sp = format(sp, "/dev/pipe_{}_{}", _getpid(), counter.fetch_add(1));

        auto name = string_view(s, (size_t)(sp.data() - s));

        utf8_to_utf16(pipename, name);

        us.Length = us.MaximumLength = (uint16_t)(utf8_to_utf16_len(name) * sizeof(char16_t));
        us.Buffer = pipename;
    }

    status = NtCreateNamedPipeFile(&server_handle, FILE_WRITE_DATA, &oa, &iosb,
                                   0, FILE_CREATE, 0, 0, 0, 0, 1, 0, 0,
                                   nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    status = NtOpenFile(&client_handle, FILE_READ_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        NtClose(server_handle);
        return -1;
    }

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].h != 0)
            continue;

        if (!client_fd.has_value()) {
            fds[i].h = client_handle;
            fds[i].cloexec = false;
            client_fd = i;
        } else {
            fds[i].h = server_handle;
            fds[i].cloexec = false;
            server_fd = i;
            break;
        }
    }

    if (client_fd.has_value() && !server_fd.has_value())
        fds[(size_t)*client_fd].h = 0;

    RtlLeaveCriticalSection(&fds_lock);

    if (!client_fd.has_value() || !server_fd.has_value()) {
        NtClose(client_handle);
        NtClose(server_handle);
        *__errno() = EMFILE;
        return -1;
    }

    pfds[0] = *client_fd;
    pfds[1] = *server_fd;

    return 0;
}

extern "C"
FILE* __popen(const char* command, const char* type) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    string_view cmd(command);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    bool write_mode;
    int pipefds[2];

    static const u16string_view DASH_C = u" -c ";

    enum class mode_type {
        read,
        write,
    };

    if (!type || !command || command[0] == 0) {
        *__errno() = EINVAL;
        return nullptr;
    }

    switch (type[0]) {
        case 'r':
            write_mode = false;
            break;

        case 'w':
            write_mode = true;
            break;

        default:
            *__errno() = EINVAL;
            return nullptr;
    }

    type++;

    while (*type != 0) {
        if (*type != 't' && *type != 'b') {
            *__errno() = EINVAL;
            return nullptr;
        }

        type++;
    }

    auto shell = getenv("SHELL");

    if (!shell || !shell[0]) {
        *__errno() = ENOENT;
        return nullptr;
    }

    auto shellsv = string_view(shell);
    auto u16shelllen = utf8_to_utf16_len(shellsv);
    auto u16cmdlen = utf8_to_utf16_len(cmd);

    auto cmdlinelen = u16shelllen + DASH_C.size() + u16cmdlen + 1;
    auto cmdline = (char16_t*)malloc(cmdlinelen * sizeof(char16_t));
    if (!cmdline) {
        *__errno() = ENOMEM;
        return nullptr;
    }

    auto u16shell = (char16_t*)malloc((u16shelllen + 1) * sizeof(char16_t));
    if (!u16shell) {
        *__errno() = ENOMEM;
        free(cmdline);
        return nullptr;
    }

    utf8_to_utf16(span(u16shell, u16shelllen), shellsv);
    u16shell[u16shelllen] = 0;

    memcpy(cmdline, u16shell, u16shelllen * sizeof(char16_t));
    memcpy(cmdline + u16shelllen, DASH_C.data(), DASH_C.size() * sizeof(char16_t));
    utf8_to_utf16(span(cmdline + u16shelllen + DASH_C.size(), u16cmdlen), cmd);
    cmdline[u16shelllen + DASH_C.size() + u16cmdlen] = 0;

    // create pipe

    if (__pipe(pipefds, 0, 0)) {
        free(u16shell);
        free(cmdline);
        return nullptr;
    }

    // launch process

    memset(&pi, 0, sizeof(pi));

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    if (write_mode) {
        si.hStdInput = (HANDLE)__get_osfhandle(pipefds[0]);
        si.hStdOutput = peb.ProcessParameters->StandardOutput;
    } else {
        si.hStdInput = peb.ProcessParameters->StandardInput;
        si.hStdOutput = (HANDLE)__get_osfhandle(pipefds[1]);
    }

    si.hStdError = peb.ProcessParameters->StandardError;

    if (!CreateProcessW(u16shell, cmdline, nullptr, nullptr, false, 0, nullptr,
                        nullptr, &si, &pi)) {
        free(u16shell);
        free(cmdline);
        __close(pipefds[0]);
        __close(pipefds[1]);

        *__errno() = EINVAL;
        return nullptr;
    }

    if (write_mode)
        __close(pipefds[0]);
    else
        __close(pipefds[1]);

    free(u16shell);
    free(cmdline);

    NtClose(pi.hThread);

    // create FILE

    auto fd = write_mode ? pipefds[1] : pipefds[0];

    auto f = new _FILE((HANDLE)__get_osfhandle(fd), fd);
    if (!f) {
        __close(fd);
        NtClose(pi.hProcess);

        *__errno() = ENOMEM;
        return nullptr;
    }

    f->proc = pi.hProcess;

    {
        RtlEnterCriticalSection(&files_lock);
        insert_tail_list(files_list, f->le);
        RtlLeaveCriticalSection(&files_lock);
    }

    return reinterpret_cast<FILE*>(f);
}

extern "C"
int __pclose(FILE* file) {
    nt_status status;
    PROCESS_BASIC_INFORMATION pbi;
    auto& f = *reinterpret_cast<_FILE*>(file);
    bool fail = false;

    if (f.proc == 0) {
        *__errno() = EBADF;
        return -1;
    }

    // close stream

    NtClose(f.h);
    f.h = 0;

    // wait for process

    while (true) {
        status = NtWaitForSingleObject(f.proc, true, nullptr);
        if (status == nt_status::USER_APC || status == nt_status::ALERTED)
            continue;

        if (success(status))
            break;

        *__errno() = nt_status_to_errno(status);
        fail = true;
        break;
    }

    if (!fail) {
        // get exit status

        status = NtQueryInformationProcess(f.proc, PROCESS_INFORMATION_CLASS::BasicInformation,
                                        &pbi, sizeof(pbi), nullptr);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            fail = true;
        }
    }

    NtClose(f.proc);

    RtlEnterCriticalSection(&files_lock);
    remove_entry_list(f.le);
    RtlLeaveCriticalSection(&files_lock);

    delete &f;

    return fail ? -1 : (int)pbi.ExitStatus;
}

extern "C"
nt_status mn_prepare_exec_block(HANDLE proc, exec_info** exec_block,
                                size_t* num_exec_block_entries) {
    unsigned int num_entries = 0;
    nt_status status;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = STDERR_FILENO + 1; i < fds.size(); i++) {
        if (fds[i].h == 0)
            continue;

        if (fds[i].cloexec)
            continue;

        num_entries++;
    }

    if (num_entries == 0) {
        RtlLeaveCriticalSection(&fds_lock);
        *exec_block = nullptr;
        *num_exec_block_entries = 0;
        return nt_status::SUCCESS;
    }

    auto entries = (exec_info*)malloc(sizeof(exec_info) * num_entries);
    if (!entries) {
        print_stderr("malloc failed\n");
        RtlLeaveCriticalSection(&fds_lock);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto e = entries;

    for (size_t i = STDERR_FILENO + 1; i < fds.size(); i++) {
        if (fds[i].h == 0)
            continue;

        if (fds[i].cloexec)
            continue;

        status = NtDuplicateObject(NtCurrentProcess(), fds[i].h, proc,
                                   &e->inherited_fd.h, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
        if (!success(status)) {
            print_stderr("NtDuplicateObject returned {}\n", status);
            free(entries);
            RtlLeaveCriticalSection(&fds_lock);
            return status;
        }

        e->type = exec_info_type::inherited_fd;
        e->inherited_fd.fd = (int)i;

        e++;
    }

    RtlLeaveCriticalSection(&fds_lock);

    *exec_block = entries;
    *num_exec_block_entries = num_entries;

    return nt_status::SUCCESS;
}

static void handle_exec_info(exec_info* info) {
    switch (info->type) {
        using enum exec_info_type;

        case inherited_fd: {
            if (info->inherited_fd.fd < 0 || (size_t)info->inherited_fd.fd >= fds.size()) {
                print_stderr("handle_exec_info: fd {} out of range\n", info->inherited_fd.fd);
                return;
            }

            RtlEnterCriticalSection(&fds_lock);

            auto& f = fds[(size_t)info->inherited_fd.fd];

            if (f.h)
                NtClose(f.h);

            f.h = info->inherited_fd.h;
            f.cloexec = false;

            RtlLeaveCriticalSection(&fds_lock);

            break;
        }

        case open: {
            auto fd = open2(info->open.path, info->open.oflag, info->open.mode);
            if (fd == -1) {
                print_stderr("_open2(\"{}\", {}, {}) failed (errno {})\n",
                             info->open.path, info->open.oflag, info->open.mode,
                             errno_to_string_view(*__errno()));
                _exit(127);
            }

            // FIXME - if stdin, stdout, or stderr, also update PEB

            if (fd != info->open.fd) {
                auto ret = __dup2(fd, info->open.fd);
                if (ret == -1) {
                    print_stderr("_dup2 failed (errno {})\n",
                                 errno_to_string_view(*__errno()));
                    _exit(127);
                }

                __close(fd);
            }

            break;
        }

        case close: {
            // FIXME - if stdin, stdout, or stderr, also update PEB

            auto ret = __close(info->close.fd);
            if (ret == -1) {
                print_stderr("_close({}) failed (errno {})\n",
                             info->close.fd, errno_to_string_view(*__errno()));
                _exit(127);
            }

            break;
        }

        case dup2: {
            // FIXME - if stdin, stdout, or stderr, also update PEB

            auto ret = __dup2(info->dup2.fd, info->dup2.newfd);
            if (ret == -1) {
                print_stderr("_dup2({}, {}) failed (errno {})\n",
                             info->dup2.fd, info->dup2.newfd,
                             errno_to_string_view(*__errno()));
                _exit(127);
            }

            break;
        }

        case chdir: {
            auto ret = __chdir(info->chdir.path);
            if (ret == -1) {
                print_stderr("_chdir(\"{}\") failed (errno {})\n",
                             info->chdir.path, errno_to_string_view(*__errno()));
                _exit(127);
            }

            break;
        }

        case fchdir: {
            auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
            HANDLE h2;
            nt_status status;

            auto h = (HANDLE)__get_osfhandle(info->fchdir.fd);
            if (h == (HANDLE)-1) {
                print_stderr("_get_osfhandle({}) failed (errno {})\n",
                             info->fchdir.fd, errno_to_string_view(*__errno()));
                _exit(127);
            }

            status = NtDuplicateObject(NtCurrentProcess(), h, NtCurrentProcess(),
                                       &h2, 0, 0, DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
            if (!success(status)) {
                print_stderr("NtDuplicateObject returned {}\n", status);
                _exit(127);
            }

            {
                mn_peb_lock lock;
                lock_guard lg(lock);

                if (!get_dos_path(h, peb.ProcessParameters->CurrentDirectory.DosPath)) {
                    NtClose(h2);
                    print_stderr("get_dos_path failed\n");
                    _exit(127);
                }

                if (peb.ProcessParameters->CurrentDirectory.Handle != 0)
                    NtClose(peb.ProcessParameters->CurrentDirectory.Handle);

                peb.ProcessParameters->CurrentDirectory.Handle = h2;
            }

            break;
        }
    }

}

extern "C"
bool mn_get_cloexec(int fd, bool* cloexec) {
    if (fd < 0 || (size_t)fd >= fds.size())
        return false;

    RtlEnterCriticalSection(&fds_lock);

    if (fds[(size_t)fd].h == 0) {
        RtlLeaveCriticalSection(&fds_lock);
        return false;
    }

    *cloexec = fds[(size_t)fd].cloexec;
    RtlLeaveCriticalSection(&fds_lock);

    return true;
}

extern "C"
bool mn_set_cloexec(int fd, bool cloexec) {
    if (fd < 0 || (size_t)fd >= fds.size())
        return false;

    RtlEnterCriticalSection(&fds_lock);

    if (fds[(size_t)fd].h == 0) {
        RtlLeaveCriticalSection(&fds_lock);
        return false;
    }

    fds[(size_t)fd].cloexec = cloexec;
    RtlLeaveCriticalSection(&fds_lock);

    return true;
}

static char mktemp_char(unsigned int i) {
    if (i < 10)
        return (char)('0' + i);
    else
        return (char)('a' + i - 10);
}

extern "C"
char* __mktemp(char* templ) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto len = strlen(templ);

    if (len < 6)
        return nullptr;

    auto sp = span(templ + len - 6, 6);

    for (auto c : sp) {
        if (c != 'X')
            return nullptr;
    }

    while (true) {
        UNICODE_STRING us;
        OBJECT_ATTRIBUTES oa;
        IO_STATUS_BLOCK iosb;
        HANDLE h;

        auto r = rand();

        sp[0] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[1] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[2] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[3] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[4] = mktemp_char((unsigned int)(r % 36));
        r /= 36;
        sp[5] = mktemp_char((unsigned int)(r % 36));

        auto sv = string_view(templ, len);
        auto u16len = utf8_to_utf16_len(sv);

        auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16pathname)
            return nullptr;

        utf8_to_utf16(span(u16pathname, u16len), sv);

        us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
        us.Buffer = u16pathname;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);
        oa.ObjectName = &us;
        oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

        auto status = NtCreateFile(&h, MAXIMUM_ALLOWED, &oa, &iosb, nullptr,
                                   FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                                   0, nullptr, 0);

        free(u16pathname);

        if (success(status)) { // already exists, try again
            NtClose(h);
            continue;
        }

        if (status != nt_status::OBJECT_NAME_NOT_FOUND)
            return nullptr;

        break;
    }

    return templ;
}
