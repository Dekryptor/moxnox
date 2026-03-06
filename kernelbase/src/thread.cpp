#include <stdint.h>

import kernelbase;
import print;

extern "C"
uint32_t TlsAlloc() {
    print_stderr("FIXME - TlsAlloc\n");

    return 0;
}
