module;

#include <wchar.h>
#include <string_view>
#include <span>
#include <optional>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/utime.h>

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
    bool error = false;
    bool eof = false;
};

static array<HANDLE, 2048> fds;

static uint8_t _stdin[sizeof(_FILE)], _stdout[sizeof(_FILE)], _stderr[sizeof(_FILE)];
static RTL_CRITICAL_SECTION files_lock, fds_lock;
static list_entry files_list = { &files_list, &files_list };

extern "C" int* __errno();

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

    EnterCriticalSection(&f.cs);
}

extern "C"
void __unlock_file(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    LeaveCriticalSection(&f.cs);
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
    InitializeCriticalSection(&cs);
}

_FILE::~_FILE() {
    DeleteCriticalSection(&cs);

    RtlEnterCriticalSection(&fds_lock);
    fds[(size_t)fd] = 0;
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

    fds[0] = peb->ProcessParameters->StandardInput;
    fds[1] = peb->ProcessParameters->StandardOutput;
    fds[2] = peb->ProcessParameters->StandardError;
}

extern "C"
FILE* __cdecl _fopen(const char* pathname, const char* mode) {
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
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;

    if (!mode || !pathname || pathname[0] == 0) {
        *__errno() = EINVAL;
        return nullptr;
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
            return nullptr;
    }

    // FIXME - implement MS' extensions?
    // FIXME - is "rb+" etc. valid?
    while (*mode != 0) {
        if (*mode != 't' && *mode != 'b') {
            *__errno() = EINVAL;
            return nullptr;
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

    auto sv = string_view(pathname);
    auto u16len = utf8_to_utf16_len(sv);

    auto u16pathname = (char16_t*)malloc(u16len * sizeof(char16_t));
    if (!u16pathname) {
        *__errno() = ENOMEM;
        return nullptr;
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
        return nullptr;
    }

    optional<int> fd;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i] == 0) {
            fds[i] = h;
            fd = i;
            break;
        }
    }

    RtlLeaveCriticalSection(&fds_lock);

    if (!fd.has_value()) {
        NtClose(h);
        *__errno() = EMFILE;
        return nullptr;
    }

    auto f = new _FILE(h, *fd);
    if (!f) {
        free(u16pathname);

        RtlEnterCriticalSection(&fds_lock);
        fds[(size_t)*fd] = 0;
        RtlLeaveCriticalSection(&fds_lock);

        NtClose(h);

        *__errno() = ENOMEM;
        return nullptr;
    }

    free(u16pathname);

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

    // FIXME - buffering

    auto status = NtReadFile(f.h, 0, nullptr, nullptr, &iosb, ptr,
                             (uint32_t)(size * n), nullptr, nullptr);
    if (status == nt_status::END_OF_FILE) {
        f.eof = true;
        return 0;
    } else if (!success(status)) {
        f.error = true;
        return 0;
    }

    if (iosb.Information < size * n)
        f.eof = true;

    return iosb.Information;
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

    static const int _SEEK_SET = 0;
    static const int _SEEK_CUR = 1;
    static const int _SEEK_END = 2;

    switch (whence) {
        case _SEEK_SET:
            start = 0;
            break;

        case _SEEK_CUR:
            start = __ftelli64_nolock(file);

            if (start == -1)
                return -1;
            break;

        case _SEEK_END:
            print("FIXME - _fseeki64_nolock SEEK_END\n");
            // FIXME - call NtQueryInformationFile with FileEndOfFileInformation
            *__errno() = EINVAL;
            return -1;

        default:
            *__errno() = EINVAL;
            return -1;
    }

    fpi.CurrentByteOffset.QuadPart = start + offset;

    auto status = NtSetInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
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

    return __mingw_vsnprintf(str, len, format, valist);
}

extern "C"
int setvbuf(FILE* /*stream*/, char* /*buffer*/, int /*mode*/, size_t /*size*/) {
    // FIXME - change buffering mode of stream?

    return 0;
}

static int do_fstat(HANDLE h, _stat64* st) {
    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_BASIC_INFORMATION fbi;
    FILE_STANDARD_INFORMATION fsi;

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

    memset(st, 0, sizeof(*st));

    static const uint32_t S_IFDIR = 040000;
    static const uint32_t S_IFREG = 0100000;

    if (fbi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        st->st_mode = S_IFDIR | 0777;
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
int _stat64(const char* path, struct _stat64* st) {
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
int _stat64i32(const char* path, struct _stat64i32* st) {
    struct _stat64 st2;

    auto ret = _stat64(path, &st2);
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
int _stat32i64(const char* path, struct _stat32i64* st) {
    struct _stat64 st2;

    auto ret = _stat64(path, &st2);
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
int _stat32(const char* path, struct _stat32* st) {
    struct _stat64 st2;

    auto ret = _stat64(path, &st2);
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
int _fstat64(int fd, struct _stat64* st) {
    HANDLE h;

    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EINVAL;
        return -1;
    }

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd];
    RtlLeaveCriticalSection(&fds_lock);

    if (!h) {
        *__errno() = EINVAL;
        return -1;
    }

    return do_fstat(h, st);
}

extern "C"
int _fstat64i32(int fd, struct _stat64i32* st) {
    struct _stat64 st2;

    auto ret = _fstat64(fd, &st2);
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
int _fstat32i64(int fd, struct _stat32i64* st) {
    struct _stat64 st2;

    auto ret = _fstat64(fd, &st2);
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
int _fstat32(int fd, struct _stat32* st) {
    struct _stat64 st2;

    auto ret = _fstat64(fd, &st2);
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
    uint32_t file_attributes, create_disposition;
    nt_status status;

    if (flags & _O_TEMPORARY) {
        print_stderr("FIXME - _open with _O_TEMPORARY\n");
        flags &= ~_O_TEMPORARY;
    }

    if (flags & _O_NOINHERIT) {
        print_stderr("FIXME - _open with _O_NOINHERIT\n");
        flags &= ~_O_NOINHERIT;
    }

    if (flags & _O_SHORT_LIVED) {
        print_stderr("FIXME - _open with _O_SHORT_LIVED\n");
        flags &= ~_O_SHORT_LIVED;
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

    if (flags & _O_APPEND) {
        print_stderr("FIXME - _open with _O_APPEND\n");
        flags &= ~_O_APPEND;
    }

    // FIXME - do we need to treat _O_TEXT and _O_BINARY differently?
    // FIXME - _O_RANDOM
    // FIXME - _O_SEQUENTIAL

    if (flags & _O_WRONLY)
        access |= GENERIC_WRITE;
    else if (flags & _O_RDWR)
        access |= GENERIC_READ | GENERIC_WRITE;
    else
        access |= GENERIC_READ;

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
                          0, nullptr, 0);
    if (!success(status)) {
        free(u16pathname);
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    free(u16pathname);

    optional<int> fd;

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i] == 0) {
            fds[i] = h;
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
int _open(const char* path, int flags, ...) {
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

extern "C"
FILE* __fdopen(int fd, const char* /*mode*/) {
    HANDLE h;

    // FIXME - do something or other with mode (read, write, append?)

    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EINVAL;
        return nullptr;
    }

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd];
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
int _chmod(const char* /*path*/, int /*flags*/) {
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
intptr_t _get_osfhandle(int fd) {
    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    HANDLE h;

    RtlEnterCriticalSection(&fds_lock);
    h = fds[(size_t)fd];
    RtlLeaveCriticalSection(&fds_lock);

    if (h == 0) {
        *__errno() = EBADF;
        return -1;
    }

    return (intptr_t)h;
}

extern "C"
int _open_osfhandle(intptr_t osfhandle, int /*flags*/) {
    optional<int> fd;

    // FIXME - flags?

    // FIXME - record last fd used somewhere, before we resort to searching the whole array?

    RtlEnterCriticalSection(&fds_lock);

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i] == 0) {
            fds[i] = osfhandle;
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
int _isatty(int fd) {
    IO_STATUS_BLOCK iosb;

    auto h = _get_osfhandle(fd);
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
int _write(int fd, const void* buffer, unsigned int count) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
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
int _read(int fd, void* buffer, size_t count) {
    IO_STATUS_BLOCK iosb;

    auto h = (HANDLE)_get_osfhandle(fd);
    if ((intptr_t)h < 0)
        return -1;

    auto status = NtReadFile(h, 0, nullptr, nullptr, &iosb, buffer,
                             (uint32_t)count, nullptr, nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return (int)iosb.Information;
}

extern "C"
int _access(const char* path, int mode) {
    print_stderr("FIXME - _access({}, {})\n", path ? path : "", mode);

    *__errno() = EINVAL;
    return -1;
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
int _setmode(int fd, int mode) {
    print("FIXME - _setmode ({}, {})\n", fd, mode); // FIXME

    return 0;
}

extern "C"
int _close(int fd) {
    if (fd < 0 || (size_t)fd >= fds.size()) {
        *__errno() = EBADF;
        return -1;
    }

    HANDLE h = 0;

    RtlEnterCriticalSection(&fds_lock);
    swap(fds[(size_t)fd], h);
    RtlLeaveCriticalSection(&fds_lock);

    if (h == 0) {
        *__errno() = EINVAL;
        return -1;
    }

    NtClose(h);

    return 0;
}
