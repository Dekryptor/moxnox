#include "kernel.h"

extern "C" void syscall(uint64_t index, uint64_t param1) {
    print("FIXME - syscall\n");
}
