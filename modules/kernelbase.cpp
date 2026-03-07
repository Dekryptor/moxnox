module;

#include <stdint.h>

export module kernelbase;

import nt;

export {

const uint32_t CTRL_C_EVENT = 0;
const uint32_t CTRL_BREAK_EVENT = 1;
const uint32_t CTRL_CLOSE_EVENT = 2;
const uint32_t CTRL_LOGOFF_EVENT = 5;
const uint32_t CTRL_SHUTDOWN_EVENT = 6;

struct EXCEPTION_POINTERS {
     EXCEPTION_RECORD* ExceptionRecord;
     CONTEXT* ContextRecord;
};

struct SECURITY_ATTRIBUTES {
     uint32_t nLength;
     void* lpSecurityDescriptor;
     bool bInheritHandle;
};

using HANDLER_ROUTINE = bool (*)(uint32_t ctrl_type);
using TOP_LEVEL_EXCEPTION_FILTER = int32_t (*)(EXCEPTION_POINTERS* exception_info);
using THREAD_START_ROUTINE = uint32_t (*)(void* ctx);

extern "C" {

bool SetConsoleCtrlHandler(HANDLER_ROUTINE func, bool add);
TOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(TOP_LEVEL_EXCEPTION_FILTER filter);
void InitializeCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void DeleteCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void EnterCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void LeaveCriticalSection(RTL_CRITICAL_SECTION* critical_section);
uint32_t TlsAlloc();
uint32_t GetLastError();
void* TlsGetValue(uint32_t tls_index);
void SetLastError(uint32_t err_code);
bool TlsSetValue(uint32_t tls_index, void* tls_value);
HANDLE CreateThread(SECURITY_ATTRIBUTES* sa, size_t stack,
                    THREAD_START_ROUTINE start, void* param, uint32_t flags,
                    uint32_t* id);

}

}
