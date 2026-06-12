#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern "C" int __mingw_vfscanf(FILE* stream, const char* format, va_list args);
extern "C" int __mingw_vsscanf(const char* str, const char* format, va_list args);

extern "C"
int __stdio_common_vfscanf(uint64_t /*options*/, FILE* stream,
                           const char* format, _locale_t /*locale*/,
                           va_list args) {
    return __mingw_vfscanf(stream, format, args);
}

extern "C"
int __stdio_common_vsscanf(uint64_t /*options*/, const char* buffer,
                           size_t /*buffer_count*/, const char* format,
                           _locale_t /*locale*/, va_list args) {
    // FIXME - buffer_count ignored (sscanf and vsscanf pass SIZE_MAX, but _snscanf passes a real length)

    return __mingw_vsscanf(buffer, format, args);
}
