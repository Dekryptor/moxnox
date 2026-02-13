module;

import ucrtbase;
import kernelbase;

#define _FILE_DEFINED
#include <string_view>
#include <span>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

export module file;

import print;
import nt;
import unicode;

using namespace std;

struct _FILE {
    _FILE(HANDLE h);
    ~_FILE();

    HANDLE h;
    RTL_CRITICAL_SECTION cs;
};

static uint8_t _stdin[sizeof(_FILE)], _stdout[sizeof(_FILE)], _stderr[sizeof(_FILE)];

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

extern "C" __declspec(dllexport)
FILE* __acrt_iob_func(unsigned int idx) {
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

extern "C" __declspec(dllexport)
void _lock_file(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    EnterCriticalSection(&f.cs);
}

extern "C" __declspec(dllexport)
void _unlock_file(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    LeaveCriticalSection(&f.cs);
}

extern "C" __declspec(dllexport)
int _fputc_nolock(int c, FILE* file) {
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
    _lock_file(file);
    auto ret = _fputc_nolock(c, file);
    _unlock_file(file);

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

    new (_stdin) _FILE(peb->ProcessParameters->StandardInput);
    new (_stdout) _FILE(peb->ProcessParameters->StandardOutput);
    new (_stderr) _FILE(peb->ProcessParameters->StandardError);
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

    return reinterpret_cast<FILE*>(f);
}

extern "C"
size_t _fread_nolock(void* ptr, size_t size, size_t n, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    IO_STATUS_BLOCK iosb;

    // FIXME - buffering

    auto status = NtReadFile(f.h, 0, nullptr, nullptr, &iosb, ptr,
                             (uint32_t)(size * n), nullptr, nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return EOF;
    }

    return iosb.Information;
}

extern "C"
size_t _fread(void* ptr, size_t size, size_t n, FILE* file) {
    _lock_file(file);
    auto ret = _fread_nolock(ptr, size, n, file);
    _unlock_file(file);

    return ret;
}

extern "C"
size_t _fwrite_nolock(const void* ptr, size_t size, size_t n, FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);
    IO_STATUS_BLOCK iosb;

    // FIXME - buffering

    auto status = NtWriteFile(f.h, 0, nullptr, nullptr, &iosb, ptr,
                              (uint32_t)(size * n), nullptr, nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return EOF;
    }

    return iosb.Information;
}

extern "C"
size_t _fwrite(const void* ptr, size_t size, size_t n, FILE* file) {
    _lock_file(file);
    auto ret = _fwrite_nolock(ptr, size, n, file);
    _unlock_file(file);

    return ret;
}

extern "C"
int _fclose(FILE* file) {
    auto& f = *reinterpret_cast<_FILE*>(file);

    // FIXME - do we need to make sure this isn't stdin, stdout, or stderr?

    NtClose(f.h);
    delete &f;

    return 0;
}

extern "C"
int _fputs(const char* s, FILE* file) {
    auto len = strlen(s);

    _lock_file(file);
    auto ret = _fwrite_nolock(s, 1, len, file);
    _unlock_file(file);

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

    _lock_file(f);
    auto ret = _fwrite_nolock(s, 1, len, f);

    if (ret != len) {
        _unlock_file(f);
        return EOF;
    }

    ret = _fwrite_nolock(&c, 1, 1, f);
    _unlock_file(f);

    if (ret != len)
        return EOF;
    else
        return 0;
}

extern "C"
int _fgetc_nolock(FILE* file) {
    print("FIXME - _fgetc_nolock\n"); // FIXME

    return 0;
}

extern "C"
int _fgetc(FILE* file) {
    _lock_file(file);
    auto ret = _fgetc_nolock(file);
    _unlock_file(file);

    return ret;
}

extern "C"
int _ungetc(int c, FILE* file) {
    print("FIXME - ungetc\n"); // FIXME

    return 0;
}

extern "C"
char* _fgets(char* s, int size, FILE* file) {
    print("FIXME - fgets\n"); // FIXME

    return nullptr;
}

extern "C"
int _fseek(FILE* file, int32_t offset, int whence) {
    print("FIXME - fseek\n"); // FIXME

    return 0;
}

extern "C"
int32_t _ftell(FILE* file) {
    print("FIXME - ftell\n"); // FIXME

    return 0;
}

extern "C"
int _remove(const char* pathname) {
    print("FIXME - remove\n"); // FIXME

    return 0;
}
