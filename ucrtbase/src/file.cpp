module;

#include <wchar.h>
#include <string_view>
#include <span>
#include <optional>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

export module file;

import print;
import nt;
import unicode;
import ntdll;
import list;
import ucrtbase;
import kernelbase;

using namespace std;

struct _FILE {
    _FILE(HANDLE h);
    ~_FILE();

    HANDLE h;
    RTL_CRITICAL_SECTION cs;
    list_entry le;
    optional<unsigned char> ungetc_char;
};

static uint8_t _stdin[sizeof(_FILE)], _stdout[sizeof(_FILE)], _stderr[sizeof(_FILE)];
static RTL_CRITICAL_SECTION files_lock;
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

_FILE::_FILE(HANDLE h) : h(h) {
    InitializeCriticalSection(&cs);
}

_FILE::~_FILE() {
    DeleteCriticalSection(&cs);
}

export void init_files() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    RtlInitializeCriticalSection(&files_lock);

    auto f1 = new (_stdin) _FILE(peb->ProcessParameters->StandardInput);
    auto f2 = new (_stdout) _FILE(peb->ProcessParameters->StandardOutput);
    auto f3 = new (_stderr) _FILE(peb->ProcessParameters->StandardError);

    insert_tail_list(files_list, f1->le);
    insert_tail_list(files_list, f2->le);
    insert_tail_list(files_list, f3->le);
}

static int nt_status_to_errno(nt_status status) {
    switch (status) {
        case nt_status::ACCESS_DENIED:
        case nt_status::SHARING_VIOLATION:
            return EACCES;

        case nt_status::NO_MORE_FILES:
        case nt_status::OBJECT_NAME_NOT_FOUND:
        case nt_status::OBJECT_PATH_NOT_FOUND:
            return ENOENT;

        case nt_status::DISK_CORRUPT_ERROR:
        case nt_status::CRC_ERROR:
        case nt_status::IO_DEVICE_ERROR:
            return EIO;

        case nt_status::INVALID_HANDLE:
            return EBADF;

        case nt_status::INSUFFICIENT_RESOURCES:
        case nt_status::NO_MEMORY:
            return ENOMEM;

        case nt_status::DISK_FULL:
            return ENOSPC;

        default:
            return EINVAL;
    }
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

    auto f = new _FILE(h);
    if (!f) {
        free(u16pathname);
        NtClose(h);
        *__errno() = ENOMEM;
        return nullptr;
    }

    f->h = h;

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
    if (!success(status))
        return 0;

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
    if (!success(status))
        return 0;

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

    // FIXME - clear EOF flag

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
        *_errno() = nt_status_to_errno(status);
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
            *_errno() = EINVAL;
            return -1;

        default:
            *_errno() = EINVAL;
            return -1;
    }

    fpi.CurrentByteOffset.QuadPart = start + offset;

    auto status = NtSetInformationFile(f.h, &iosb, &fpi, sizeof(fpi),
                                       FILE_INFORMATION_CLASS::PositionInformation);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    f.ungetc_char = nullopt;

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
int _remove(const char* /*pathname*/) {
    print("FIXME - remove\n"); // FIXME

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
