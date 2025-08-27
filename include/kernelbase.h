#pragma once

#include <stdint.h>

static const uint32_t CTRL_C_EVENT = 0;
static const uint32_t CTRL_BREAK_EVENT = 1;
static const uint32_t CTRL_CLOSE_EVENT = 2;
static const uint32_t CTRL_LOGOFF_EVENT = 5;
static const uint32_t CTRL_SHUTDOWN_EVENT = 6;

using PHANDLER_ROUTINE = bool (*)(uint32_t dwCtrlType);

extern "C" {
    bool SetConsoleCtrlHandler(PHANDLER_ROUTINE func, bool add);
}
