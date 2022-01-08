#include <stdint.h>

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}
