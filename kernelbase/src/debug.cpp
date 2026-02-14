import kernelbase;
import print;

extern "C" __declspec(dllexport)
TOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(TOP_LEVEL_EXCEPTION_FILTER filter) {
    print("FIXME - SetUnhandledExceptionFilter\n");

    return nullptr;
}
