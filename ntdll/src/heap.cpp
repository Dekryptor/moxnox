#include "ntdll_private.h"

extern "C" __declspec(dllexport)
void* RtlCreateHeap(uint32_t flags, void* heap_base, size_t reserve_size, size_t commit_size,
                    void* lock, RTL_HEAP_PARAMETERS* parameters) {
    print("FIXME - RtlCreateHeap\n");

    return nullptr;
}

extern "C" __declspec(dllexport)
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size) {
    print("FIXME - RtlAllocateHeap\n");

    return nullptr;
}

extern "C" __declspec(dllexport)
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address) {
    print("FIXME - RtlFreeHeap\n");

    return true;
}

extern "C" __declspec(dllexport)
void* RtlDestroyHeap(void* heap_handle) {
    print("FIXME - RtlDestroyHeap\n");

    return nullptr;
}
