#include <stdint.h>
#include <winerror.h>

import nt;
import ntdll;
import kernelbase;
import print;

extern "C"
bool GetModuleHandleExW(uint32_t flags, const char16_t* name, HMODULE* mod) {
    static const uint32_t supported_flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

    if (flags & ~supported_flags) {
        print_stderr("GetModuleHandleExW: unsupported flags {:x}\n",
                     flags & ~supported_flags);
        *mod = nullptr;
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (!(flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) {
        print("FIXME - GetModuleHandleExW with flags {:x}\n", flags);
        *mod = nullptr;
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    void* base;

    if (name) {
        if (!RtlPcToFileHeader(name, &base)) {
            SetLastError(ERROR_DLL_NOT_FOUND);
            return false;
        }
    } else
        base = NtCurrentTeb()->ProcessEnvironmentBlock->ImageBaseAddress;

    if (!(flags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT)) {
        if (auto status = LdrAddRefDll(0, base); !success(status)) {
            *mod = nullptr;
            SetLastError(RtlNtStatusToDosError(status));
            return false;
        }
    }

    *mod = base;

    return true;
}

extern "C"
HMODULE GetModuleHandleW(const char16_t* mod) {
    HMODULE ret;

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, mod, &ret);

    return ret;
}

extern "C"
bool FreeLibrary(HMODULE /*mod*/) {
    print("FIXME - FreeLibrary\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return false;
}
