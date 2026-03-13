#include <stdint.h>
#include <winerror.h>

import nt;
import kernelbase;
import print;

extern "C"
bool GetModuleHandleExW(uint32_t /*flags*/, const char16_t* /*name*/,
                        HMODULE* mod) {
    print("FIXME - GetModuleHandleExW\n");

    *mod = nullptr;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return false;
}

extern "C"
HMODULE GetModuleHandleW(const char16_t* mod) {
    HMODULE ret;

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, mod, &ret);

    return ret;
}
