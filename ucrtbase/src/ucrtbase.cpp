#include <string.h>
#include <stdint.h>

import nt;
import ntdll;

extern "C" __declspec(dllexport)
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C" __declspec(dllexport)
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

extern "C" __declspec(dllexport)
void* calloc(size_t nmemb, size_t size) {
    // FIXME - should test for overflow of nmemb * size

    auto ptr = malloc(nmemb * size);
    if (!ptr)
        return nullptr;

    memset(ptr, 0, nmemb * size);

    return ptr;
}

extern "C" __declspec(dllexport)
void* realloc(void* ptr, size_t size) {
    return RtlReAllocateHeap(GetProcessHeap(), 0, ptr, size);
}

// copied from musl: src/math/floorf.c
extern "C" __declspec(dllexport)
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
extern "C" __declspec(dllexport)
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

extern "C" __declspec(dllexport)
void abort() {
    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)0xffffffff);
    __builtin_unreachable();
}

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
