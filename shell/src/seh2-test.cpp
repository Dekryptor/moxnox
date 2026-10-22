// SPDX-License-Identifier: GPL-2.0-only
#include <windows.h>

import print;

volatile int g_finally_ran = 0;
volatile int g_handler_ran = 0;

int main() {
    print("=== Testing C SEH ===\n");

    // Test 1: Basic __except
    __try {
        print("1. In __try block\n");
        RaiseException(0xe0000001, 0, 0, nullptr);
        print("1. After exception (shouldn't print)\n");
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        print("1. In __except handler\n");
        g_handler_ran = 1;
    }

    // Test 2: Filter function
    __try {
        print("\n2. About to raise exception\n");
        RaiseException(0xe0000002, 0, 0, nullptr);
    }
    __except(
        print("2. Filter executing, code=0x{:x}\n", GetExceptionCode()),
        EXCEPTION_EXECUTE_HANDLER
    ) {
        print("2. In handler\n");
    }

    // Test 3: __finally
    __try {
        print("\n3. In __try, about to leave normally\n");
    }
    __finally {
        print("3. In __finally (normal exit)\n");
        g_finally_ran = 1;
    }

    // Test 4: __finally with exception
    __try {
        __try {
            print("\n4. Inner __try, raising exception\n");
            RaiseException(0xe0000003, 0, 0, nullptr);
        }
        __finally {
            print("4. Inner __finally during unwind\n");
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        print("4. Outer handler caught it\n");
    }

    print("\n=== Results ===\n");
    print("Handler ran: {}\n", g_handler_ran);
    print("Finally ran: {}\n", g_finally_ran);

    return 0;
}
