/*
 * posix-spawn-test - M0: posix_spawn + waitpid mechanics (no exec yet).
 *
 * Re-spawns itself in a "child mode" to create children with known behaviour,
 * then exercises the wait path: exit-status plumbing, pid matching, WNOHANG
 * before/after exit, and ECHILD once a child has been reaped. Later milestones
 * (exec, fd inheritance, exec* variants) grow onto this harness.
 *
 * Child modes are dispatched on argv before any driver work, so the same binary
 * serves as both the test driver and the thing being spawned.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <string_view>
#include <span>
#include <type_traits>
#include <utility>
#include <sys/types.h>
#include <sys/wait.h>
#include <spawn.h>

import nt;
import unicode;
import print;
import format;

using namespace std;

static unsigned int failures = 0;

// Forwards to print(). The format string is taken as print's own consteval
// `checker` type (wrapped in type_identity_t so Args deduce from the trailing
// arguments, not the format), so the compile-time format check still happens at
// the call site; the constructed checker is then handed straight to print().
template<typename... Args>
static void check(bool ok, type_identity_t<checker<type_identity_t<Args>...>> fmt,
                  Args&&... args) {
    if (ok)
        print("PASS: ");
    else {
        print("FAIL: ");
        failures++;
    }
    print(fmt, forward<Args>(args)...);
    print("\n");
}

// Our own image path, from the PEB, as UTF-8 - so we can re-spawn ourselves
// without hardcoding an install location.
static const char* self_path() {
    static char buf[4096];
    auto& ipn = NtCurrentTeb()->ProcessEnvironmentBlock->ProcessParameters->ImagePathName;
    u16string_view u16(ipn.Buffer, ipn.Length / sizeof(char16_t));
    size_t len = utf16_to_utf8_len(u16);
    if (len >= sizeof(buf))
        return nullptr;
    utf16_to_utf8(span<char>(buf, len), u16);
    buf[len] = 0;
    return buf;
}

static void sleep_ms(unsigned int ms) {
    LARGE_INTEGER delay;
    delay.QuadPart = -(int64_t)ms * 10000;  // relative, 100ns units
    NtDelayExecution(false, &delay);
}

static int spawn_self(const char* self, char* const argv[], pid_t* pid) {
    // The child doesn't read its environment; a minimal one keeps this off the
    // (separately-tested) `environ` machinery.
    char* env[] = { (char*)"PATH=/bin", nullptr };
    return posix_spawn(pid, self, nullptr, nullptr, argv, env);
}

// Spawn a child that exits 99: waitpid must report exactly that, and once it's
// been reaped a second wait on the same pid must fail with ECHILD.
static void test_exit_status(const char* self) {
    char* cargv[] = { (char*)self, (char*)"--exit", (char*)"99", nullptr };
    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    check(err == 0, "posix_spawn (exit 99) returned {}", err);
    if (err != 0)
        return;

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    check(w == pid, "waitpid returned {} (expected pid {})", (int)w, (int)pid);
    check(WIFEXITED(status), "child exited normally");
    check(WIFEXITED(status) && WEXITSTATUS(status) == 99,
          "exit status is 99 (got {})", WEXITSTATUS(status));

    errno = 0;
    status = 0;
    pid_t w2 = waitpid(pid, &status, 0);
    check(w2 == -1 && errno == ECHILD,
          "second waitpid gives -1/ECHILD (got {}, errno {})", (int)w2, errno);
}

// WNOHANG must return 0 while the child is still running, then reap it normally.
static void test_wnohang(const char* self) {
    char* cargv[] = { (char*)self, (char*)"--sleep-exit", (char*)"500", (char*)"7", nullptr };
    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    check(err == 0, "posix_spawn (sleep-exit) returned {}", err);
    if (err != 0)
        return;

    int status = 0;
    pid_t w = waitpid(pid, &status, WNOHANG);
    check(w == 0, "WNOHANG while child runs returns 0 (got {})", (int)w);

    // Now block until it finishes and reap it.
    status = 0;
    w = waitpid(pid, &status, 0);
    check(w == pid && WIFEXITED(status) && WEXITSTATUS(status) == 7,
          "blocking waitpid reaps with status 7 (pid {}, status {})",
          (int)w, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

int main(int argc, char** argv) {
    // --- Child modes (keep first and minimal) ---
    if (argc >= 3 && !strcmp(argv[1], "--exit"))
        return atoi(argv[2]);

    if (argc >= 4 && !strcmp(argv[1], "--sleep-exit")) {
        sleep_ms((unsigned int)atoi(argv[2]));
        return atoi(argv[3]);
    }

    // --- Driver ---
    const char* self = self_path();
    if (!self) {
        print("FAIL: could not determine own path\n");
        return 1;
    }
    print("self: {}\n", self);

    test_exit_status(self);
    test_wnohang(self);

    print("\n{} ({} failure{})\n", failures ? "FAILED" : "OK",
          failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
