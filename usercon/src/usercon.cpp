#include <ntdll.h>

using namespace std;

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

extern "C"
void startup() {
    // FIXME

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
