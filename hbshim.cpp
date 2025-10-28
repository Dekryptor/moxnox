#include <string.h>
#include <stdint.h>
#include <stdlib.h>

import nt;

extern "C"
int __mingw_vsnprintf(char* /*_DstBuf*/, size_t /*_MaxCount*/, const char* /*_Format*/,
                      va_list /*_ArgList*/) {
    // FIXME
    return 0;
}

extern "C"
int __mingw_snprintf(char* /*s*/, size_t /*n*/, const char* /*format*/, ...) {
    // FIXME
    return 0;
}

extern "C"
int DllMainCRTStartup(HANDLE hinstDLL, uint32_t dwReason, void* lpvReserved) {
    return 1;
}
