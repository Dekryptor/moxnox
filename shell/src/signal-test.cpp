#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

import nt;
import kernelbase;
import unique_handle;
import print;

using namespace std;

// --- Child process for test 4 ---

static volatile int child_got_signal = 0;

static void child_sigint_handler(int) {
    child_got_signal = 1;
}

static void run_child() {
    signal(SIGINT, child_sigint_handler);

    while (!child_got_signal) {
        LARGE_INTEGER timeout;

        timeout.QuadPart = 0;

        NtDelayExecution(true, &timeout);
    }

    exit(42);
}

// --- Test 1: signal() + raise() ---

static int test1_count = 0;

static void test1_handler(int sig) {
    if (sig == SIGINT)
        test1_count++;
}

static void test_signal_and_raise() {
    print("=== Test 1: signal() and raise() ===\n");

    signal(SIGINT, test1_handler);
    raise(SIGINT);

    if (test1_count != 1) {
        print("Test 1: expected handler to run once, got {}\n", test1_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_IGN);
    raise(SIGINT);

    if (test1_count != 1) {
        print("Test 1: SIG_IGN handler still ran (count = {})\n", test1_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 1 passed\n\n");
}

// --- Test 2: sigprocmask blocking ---

static int test2_count = 0;

static void test2_handler(int) {
    test2_count++;
}

static void test_sigprocmask_blocking() {
    print("=== Test 2: sigprocmask blocking ===\n");

    signal(SIGINT, test2_handler);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    raise(SIGINT);

    if (test2_count != 0) {
        print("Test 2: handler ran while signal was blocked (count = {})\n", test2_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    sigprocmask(SIG_UNBLOCK, &mask, nullptr);

    if (test2_count != 1) {
        print("Test 2: expected handler to run once on unblock, got {}\n", test2_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 2 passed\n\n");
}

// --- Test 3: blocked signal raised twice fires exactly once ---

static int test3_count = 0;

static void test3_handler(int) {
    test3_count++;
}

static void test_pending_once() {
    print("=== Test 3: blocked signal raised twice fires once on unblock ===\n");

    signal(SIGINT, test3_handler);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    raise(SIGINT);
    raise(SIGINT);

    if (test3_count != 0) {
        print("Test 3: handler ran while signal was blocked (count = {})\n", test3_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    sigprocmask(SIG_UNBLOCK, &mask, nullptr);

    if (test3_count != 1) {
        print("Test 3: expected handler to run exactly once, got {}\n", test3_count);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    signal(SIGINT, SIG_DFL);
    print("Test 3 passed\n\n");
}

// --- Test 4: kill() cross-process ---

static void test_kill_cross_process() {
    print("=== Test 4: kill() cross-process ===\n");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    char16_t cmdline[] = u"signal-test --child";
    if (!CreateProcessW(u"/bin/signal-test.exe", cmdline,
                        nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi)) {
        print("Test 4: CreateProcessW failed\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    unique_handle proc{pi.hProcess};
    NtClose(pi.hThread);

    Sleep(100);

    if (kill(pi.dwProcessId, SIGINT) != 0) {
        print("Test 4: kill() failed\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    NtWaitForSingleObject(proc.get(), false, nullptr);

    PROCESS_BASIC_INFORMATION pbi;
    NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                              &pbi, sizeof(pbi), nullptr);

    if ((uint32_t)pbi.ExitStatus != 42) {
        print("Test 4: expected child exit code 42, got {}\n", (uint32_t)pbi.ExitStatus);
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    print("Test 4 passed\n\n");
}

int main(int argc, char* argv[]) {
    if (argc >= 2 && strcmp(argv[1], "--child") == 0) {
        run_child();
        return 0;
    }

    print("=== Signal test suite ===\n\n");

    test_signal_and_raise();
    test_sigprocmask_blocking();
    test_pending_once();
    test_kill_cross_process();

    print("All tests passed\n");
    return 0;
}
