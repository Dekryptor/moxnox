#include <ntdll.h>

extern "C"
int DllMainCRTStartup(HANDLE hinstDLL, uint32_t dwReason, void* lpvReserved) {
    return 1;
}
