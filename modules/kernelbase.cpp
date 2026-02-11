module;

#include <stdint.h>

export module kernelbase;

export {

const uint32_t CTRL_C_EVENT = 0;
const uint32_t CTRL_BREAK_EVENT = 1;
const uint32_t CTRL_CLOSE_EVENT = 2;
const uint32_t CTRL_LOGOFF_EVENT = 5;
const uint32_t CTRL_SHUTDOWN_EVENT = 6;

using HANDLER_ROUTINE = bool (*)(uint32_t ctrl_type);

extern "C" {

bool SetConsoleCtrlHandler(HANDLER_ROUTINE func, bool add);

}

}
