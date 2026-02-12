module;

import ucrtbase;
import kernelbase;

#define _FILE_DEFINED
#include <string_view>
#include <stdint.h>

export module file;

import print;
import nt;

using namespace std;

struct _FILE {
    _FILE(HANDLE h);
    ~_FILE();

    HANDLE h;
    RTL_CRITICAL_SECTION cs;
};

static uint8_t _stdin[sizeof(_FILE)], _stdout[sizeof(_FILE)], _stderr[sizeof(_FILE)];

extern "C" __declspec(dllexport)
int puts(const char* s) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;
    auto sv = string_view(s);

    // FIXME - implement using fwrite and stdout

    auto status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                              nullptr, nullptr, &iosb, sv.data(),
                              (uint32_t)sv.size(), nullptr, nullptr);
    if (!success(status))
        return EOF;

    status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                         nullptr, nullptr, &iosb, "\n", 1, nullptr,
                         nullptr);
    if (!success(status))
        return EOF;

    return 0;
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

extern "C"
FILE* __cdecl _fopen(const char* pathname, const char* mode) {
    print("FIXME - fopen\n");

    return nullptr;
}
