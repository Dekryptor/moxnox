#include <ntdll.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern "C"
void* realloc(void* ptr, size_t size) {
    return RtlReAllocateHeap(GetProcessHeap(), 0, ptr, size);
}

extern "C"
void __imp__assert(const char* /*_Message*/, const char* /*_File*/, unsigned /*_Line*/) {
    // FIXME
}

extern "C"
int __mingw_vsnprintf(char* /*_DstBuf*/, size_t /*_MaxCount*/, const char* /*_Format*/,
                      va_list /*_ArgList*/) {
    // FIXME
    return 0;
}

extern "C"
int __mingw_snprintf(char* /*s*/, size_t /*n*/, const char* /*format*/, ...) {
    // FIXME
    return 0;
}

// copied from musl: src/math/floorf.c
extern "C"
float floorf(float x) {
    union {float f; uint32_t i;} u = {x};

    int e = (int)(u.i >> 23 & 0xff) - 0x7f;
    uint32_t m;

    if (e >= 23)
        return x;
    if (e >= 0) {
        m = 0x007fffff >> e;
        if ((u.i & m) == 0)
            return x;
        if (u.i >> 31)
            u.i += m;
        u.i &= ~m;
    } else {
        if (u.i >> 31 == 0)
            u.i = 0;
        else if (u.i << 1)
            u.f = -1.0;
    }
    return u.f;
}

// copied from musl: src/math/floorf.c
extern "C"
float ceilf(float x) {
    union {float f; uint32_t i;} u = {x};
    int e = (int)(u.i >> 23 & 0xff) - 0x7f;
    uint32_t m;

    if (e >= 23)
        return x;
    if (e >= 0) {
        m = 0x007fffff >> e;
        if ((u.i & m) == 0)
            return x;
        if (u.i >> 31 == 0)
            u.i += m;
        u.i &= ~m;
    } else {
        if (u.i >> 31)
            u.f = -0.0;
        else if (u.i << 1)
            u.f = 1.0;
    }
    return u.f;
}

extern "C"
int DllMainCRTStartup(HANDLE hinstDLL, uint32_t dwReason, void* lpvReserved) {
    return 1;
}
