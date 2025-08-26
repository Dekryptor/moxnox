#include <nt.h>
#include <ntdll.h>

extern "C" __declspec(dllexport)
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C" __declspec(dllexport)
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
