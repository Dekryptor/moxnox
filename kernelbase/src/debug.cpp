#include <utility>
#include <stdint.h>

import kernelbase;
import print;

using namespace std;

static TOP_LEVEL_EXCEPTION_FILTER unhandled_filter = nullptr;

extern "C"
TOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(TOP_LEVEL_EXCEPTION_FILTER filter) {
    swap(filter, unhandled_filter);

    return filter;
}

extern "C"
void RaiseException(uint32_t /*code*/, uint32_t /*flags*/, uint32_t /*count*/,
                    const uintptr_t* /*args*/) {
    print_stderr("FIXME - RaiseException\n"); // FIXME
}
