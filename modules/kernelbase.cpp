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

using HANDLER_ROUTINE = bool (*)(uint32_t ctrl_type);
using TOP_LEVEL_EXCEPTION_FILTER = int32_t (*)(EXCEPTION_POINTERS* exception_info);

extern "C" {

bool SetConsoleCtrlHandler(HANDLER_ROUTINE func, bool add);
TOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(TOP_LEVEL_EXCEPTION_FILTER filter);
void InitializeCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void DeleteCriticalSection(RTL_CRITICAL_SECTION* critical_section);

}

}
