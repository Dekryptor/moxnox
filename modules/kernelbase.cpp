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

const uint32_t CREATE_SUSPENDED = 0x00000004;

struct EXCEPTION_POINTERS {
     EXCEPTION_RECORD* ExceptionRecord;
     CONTEXT* ContextRecord;
};

struct SECURITY_ATTRIBUTES {
     uint32_t nLength;
     void* lpSecurityDescriptor;
     bool bInheritHandle;
};

struct STARTUPINFOW {
     uint32_t cb;
     char16_t* lpReserved;
     char16_t* lpDesktop;
     char16_t* lpTitle;
     uint32_t dwX;
     uint32_t dwY;
     uint32_t dwXSize;
     uint32_t dwYSize;
     uint32_t dwXCountChars;
     uint32_t dwYCountChars;
     uint32_t dwFillAttribute;
     uint32_t dwFlags;
     uint16_t wShowWindow;
     uint16_t cbReserved2;
     uint8_t* lpReserved2;
     HANDLE hStdInput;
     HANDLE hStdOutput;
     HANDLE hStdError;
};

const uint32_t STARTF_USESHOWWINDOW = 0x00000001;
const uint32_t STARTF_USESIZE = 0x00000002;
const uint32_t STARTF_USEPOSITION = 0x00000004;
const uint32_t STARTF_USECOUNTCHARS = 0x00000008;
const uint32_t STARTF_USEFILLATTRIBUTE = 0x00000010;
const uint32_t STARTF_RUNFULLSCREEN = 0x00000020;
const uint32_t STARTF_FORCEONFEEDBACK = 0x00000040;
const uint32_t STARTF_FORCEOFFFEEDBACK = 0x00000080;
const uint32_t STARTF_USESTDHANDLES = 0x00000100;
const uint32_t STARTF_USEHOTKEY = 0x00000200;
const uint32_t STARTF_TITLEISLINKNAME = 0x00000800;
const uint32_t STARTF_TITLEISAPPID = 0x00001000;
const uint32_t STARTF_PREVENTPINNING = 0x00002000;
const uint32_t STARTF_UNTRUSTEDSOURCE = 0x00008000;
const uint32_t STARTF_HOLOGRAPHIC = 0x00040000;

struct PROCESS_INFORMATION {
     HANDLE hProcess;
     HANDLE hThread;
     uint32_t dwProcessId;
     uint32_t dwThreadId;
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
bool CreateProcessW(const char16_t* application_name, char16_t* command_line,
                    const SECURITY_ATTRIBUTES* process_attributes,
                    const SECURITY_ATTRIBUTES* thread_attributes,
                    bool inherit_handles, uint32_t creation_flags,
                    const void* environment, const char16_t* current_directory,
                    const STARTUPINFOW* startup_info,
                    PROCESS_INFORMATION* process_information);

}

}
