import ucrtbase;

#define _FILE_DEFINED
#include <string_view>
#include <stdint.h>

import print;
import nt;

using namespace std;

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
    print("FIXME - __acrt_iob_func\n");

    return nullptr;
}

extern "C" __declspec(dllexport)
void _lock_file(FILE* file) {
    print("FIXME - _lock_file\n");
}

extern "C" __declspec(dllexport)
void _unlock_file(FILE* file) {
    print("FIXME - _unlock_file\n");
}

extern "C" __declspec(dllexport)
int fputc(int c, FILE* stream) {
    print("FIXME - fputc\n");

    return 0;
}
