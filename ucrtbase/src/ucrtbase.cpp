#include <nt.h>
#include <ntdll.h>
#include <string.h>

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

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
