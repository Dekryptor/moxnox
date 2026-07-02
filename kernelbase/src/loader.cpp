#include <stdint.h>
#include <winerror.h>
#include <string.h>
#include <string_view>

import nt;
import ntdll;
import kernelbase;
import print;

using namespace std;

extern "C"
bool GetModuleHandleExW(uint32_t flags, const char16_t* name, HMODULE* mod) {
    static const uint32_t supported_flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

    if (flags & ~supported_flags) {
        print_stderr("GetModuleHandleExW: unsupported flags {:x}\n",
                     flags & ~supported_flags);
        *mod = nullptr;
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    if (!(flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) {
        print("FIXME - GetModuleHandleExW with flags {:x}\n", flags);
        *mod = nullptr;
        RtlSetLastWin32Error(ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    void* base;

    if (name) {
        if (!RtlPcToFileHeader(name, &base)) {
            RtlSetLastWin32Error(ERROR_DLL_NOT_FOUND);
            return false;
        }
    } else
        base = NtCurrentTeb()->ProcessEnvironmentBlock->ImageBaseAddress;

    if (!(flags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT)) {
        if (auto status = LdrAddRefDll(0, base); !success(status)) {
            *mod = nullptr;
            RtlSetLastWin32Error(RtlNtStatusToDosError(status));
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
bool FreeLibrary(HMODULE mod) {
    auto status = LdrUnloadDll(mod);

    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return false;
    }

    return true;
}

extern "C"
void* GetProcAddress(HMODULE mod, const char* function) {
    nt_status status;
    void* addr;
    ANSI_STRING as;

    if (!mod) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return nullptr;
    }

    // FIXME - resolving by ordinal

    as.Buffer = (char*)function;
    as.Length = as.MaximumLength = (uint16_t)strlen(function);

    status = LdrGetProcedureAddress(mod, &as, 0, &addr);
    if (!success(status)) {
        RtlSetLastWin32Error(RtlNtStatusToDosError(status));
        return nullptr;
    }

    return addr;
}
