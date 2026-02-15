#include <string.h>
#include <stdint.h>

import nt;
import ntdll;
import print;
import file;

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

extern "C"
void* calloc(size_t nmemb, size_t size) {
    // FIXME - should test for overflow of nmemb * size

    auto ptr = malloc(nmemb * size);
    if (!ptr)
        return nullptr;

    memset(ptr, 0, nmemb * size);

    return ptr;
}

extern "C"
void* realloc(void* ptr, size_t size) {
    return RtlReAllocateHeap(GetProcessHeap(), 0, ptr, size);
}

extern "C"
void abort() {
    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)0xffffffff);
    __builtin_unreachable();
}

extern "C"
void _assert(const char* /*str*/, const char* /*file*/, unsigned int /*line*/) {
    // FIXME - print message
    abort();
}

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t reason, void*) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            init_files();
            break;

        case DLL_PROCESS_DETACH:
            _fflush(nullptr);
            break;

        default:
            break;
    }

    return 1;
}
