#include <stdint.h>

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}

extern "C" __declspec(dllexport)
void mn_start_process(void* entry_point) {
    // FIXME - load DLLs (recursively)
    // FIXME - resolve imports
    // FIXME - relocations

    __asm __volatile(
        "jmp %0"
        :
        : "a" (entry_point)
    );
}
