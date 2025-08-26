#include <ntdll.h>

extern "C"
void abort() {
    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)0xffffffff);
    __builtin_unreachable();
}

extern "C"
int DllMainCRTStartup(HANDLE hinstDLL, uint32_t dwReason, void* lpvReserved) {
    return 1;
}
