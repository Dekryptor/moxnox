#include <stdint.h>
#include <atomic>

import ucrtbase;
import print;

using namespace std;

extern "C" int* __errno();

static atomic<__sighandler_t> sigabrt_handler = SIG_DFL;
static atomic<__sighandler_t> sigfpe_handler = SIG_DFL;
static atomic<__sighandler_t> sigill_handler = SIG_DFL;
static atomic<__sighandler_t> sigint_handler = SIG_DFL;
static atomic<__sighandler_t> sigsegv_handler = SIG_DFL;
static atomic<__sighandler_t> sigterm_handler = SIG_DFL;

extern "C"
__sighandler_t signal(int sig, __sighandler_t func) {
    switch (sig) {
        case SIGABRT:
            return sigabrt_handler.exchange(func);

        case SIGFPE:
            return sigfpe_handler.exchange(func);

        case SIGILL:
            return sigill_handler.exchange(func);

        case SIGINT:
            return sigint_handler.exchange(func);

        case SIGSEGV:
            return sigsegv_handler.exchange(func);

        case SIGTERM:
            return sigterm_handler.exchange(func);
    }

    return SIG_ERR;
}

extern "C"
int raise(int sig) {
    print("FIXME - raise {}\n", sig);

    *__errno() = EINVAL;
    return 1;
}
