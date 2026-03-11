#include <stdint.h>
#include <winerror.h>

import nt;
import kernelbase;
import print;

extern "C"
bool GetModuleHandleExW(uint32_t /*flags*/, const char16_t* /*name*/,
                        HMODULE* /*mod*/) {
    print("FIXME - GetModuleHandleExW\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
}
