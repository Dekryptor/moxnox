#include <utility>

import ucrtbase;
import print;

using namespace std;

static int new_mode;

extern "C"
int _set_new_mode(int mode) {
    swap(mode, new_mode);

    return mode;
}

// FIXME - Clang leaks header file from format module?
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-dllimport"

extern "C"
void* _aligned_malloc(size_t size, size_t alignment) {
    print_stderr("FIXME - _aligned_malloc ({:x}, {:x})\n", size, alignment);

    return nullptr;
}

#pragma clang diagnostic pop
