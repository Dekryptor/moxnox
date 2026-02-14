#include <utility>

import kernelbase;

using namespace std;

static TOP_LEVEL_EXCEPTION_FILTER unhandled_filter = nullptr;

extern "C" __declspec(dllexport)
TOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(TOP_LEVEL_EXCEPTION_FILTER filter) {
    swap(filter, unhandled_filter);

    return filter;
}
