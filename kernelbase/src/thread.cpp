#include <stdint.h>

import kernelbase;
import print;

extern "C"
uint32_t TlsAlloc() {
    print_stderr("FIXME - TlsAlloc\n");

    return 0;
}

extern "C"
uint32_t GetLastError() {
    print_stderr("FIXME - GetLastError\n");

    return 0;
}

extern "C"
void* TlsGetValue(uint32_t tls_index) {
    print_stderr("FIXME - TlsGetValue {}\n", tls_index);

    return nullptr;
}

extern "C"
bool TlsSetValue(uint32_t tls_index, void* tls_value) {
    print_stderr("FIXME - TlsSetValue {}, {:x}\n", tls_index, (uintptr_t)tls_value);

    return false;
}
