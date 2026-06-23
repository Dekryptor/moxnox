/*
 * spawn-test - process primitives: posix_spawn, waitpid, and exec.
 *
 * Re-spawns itself in a "child mode" to create children with known behaviour,
 * then exercises the wait path: exit-status plumbing, pid matching, WNOHANG
 * before/after exit, and ECHILD once a child has been reaped (M0).
 *
 * M1 adds exec: a spawned child execve()s itself into a second mode, and we
 * check the pid is preserved across the exec - the original pid reaps the
 * exec'd image, which still sees its own pid and its parent unchanged. Nothing
 * is asserted about fd inheritance yet (that is M2), though stage2 does print a
 * diagnostic line and so leans on the standard streams surviving.
 *
 * Child modes are dispatched on argv before any driver work, so the same binary
 * serves as the test driver, the thing being spawned, and the thing exec'd into.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <string_view>
#include <span>
#include <type_traits>
#include <utility>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>

import nt;
import unicode;
import print;
import format;

using namespace std;

static unsigned int failures = 0;

// Exit code stage1 returns if execve() comes back at all (i.e. failed). Kept
// outside the 0..3 range the exec success path uses so the driver can tell the
// two apart.
static constexpr int exec_failed_marker = 99;

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

// stage2: the image we exec'd *into*. Verify the exec preserved our identity,
// then report the result through the exit code (bit 0 = pid ok, bit 1 = ppid ok)
// so the driver can reap it via waitpid.
static int do_exec_stage2(const char* expected_pid, const char* expected_ppid) {
    int exp_pid = atoi(expected_pid);
    int exp_ppid = atoi(expected_ppid);
    int got_pid = getpid();
    int got_ppid = (int)getppid();

    print("stage2: getpid={} (expected {}), getppid={} (expected {})\n",
          got_pid, exp_pid, got_ppid, exp_ppid);

    int result = 0;
    if (got_pid == exp_pid)
        result |= 1;
    if (got_ppid == exp_ppid)
        result |= 2;

    return result;
}

// stage1: the child the driver spawned. Record our pid, then exec into stage2,
// passing it our (pre-exec) pid and the driver's pid to check against. A
// successful exec never returns; reaching the end means execve failed.
static int do_exec_stage1(const char* expected_ppid) {
    const char* self = self_path();
    if (!self) {
        print("stage1: could not determine own path\n");
        return exec_failed_marker;
    }

    char pid_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());

    char* eargv[] = { (char*)self, (char*)"--exec-stage2", pid_str,
                      (char*)expected_ppid, nullptr };
    char* env[] = { (char*)"PATH=/bin", nullptr };

    execve(self, eargv, env);

    return exec_failed_marker;
}

// Spawn a child that execs itself: the original pid must reap the exec'd image,
// and that image must see its pid and ppid unchanged across the exec.
static void test_exec_pid_preserved(const char* self) {
    char ppid_str[16];
    snprintf(ppid_str, sizeof(ppid_str), "%d", getpid());

    char* cargv[] = { (char*)self, (char*)"--exec-stage1", ppid_str, nullptr };
    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    check(err == 0, "posix_spawn (exec-stage1) returned {}", err);
    if (err != 0)
        return;

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    check(w == pid, "waitpid after exec returns original pid (got {}, expected {})",
          (int)w, (int)pid);
    check(WIFEXITED(status), "exec'd child exited normally");
    if (!WIFEXITED(status))
        return;

    int code = WEXITSTATUS(status);
    if (code == exec_failed_marker) {
        check(false, "execve in child succeeded (child reported exec failure)");
        return;
    }

    check((code & 1) != 0, "getpid() preserved across exec");
    check((code & 2) != 0, "getppid() preserved across exec");
}

int main(int argc, char** argv) {
    // --- Child modes (keep first and minimal) ---
    if (argc >= 3 && !strcmp(argv[1], "--exit"))
        return atoi(argv[2]);

    if (argc >= 4 && !strcmp(argv[1], "--sleep-exit")) {
        sleep_ms((unsigned int)atoi(argv[2]));
        return atoi(argv[3]);
    }

    if (argc >= 3 && !strcmp(argv[1], "--exec-stage1"))
        return do_exec_stage1(argv[2]);

    if (argc >= 4 && !strcmp(argv[1], "--exec-stage2"))
        return do_exec_stage2(argv[2], argv[3]);

    // --- Driver ---
    const char* self = self_path();
    if (!self) {
        print("FAIL: could not determine own path\n");
        return 1;
    }
    print("self: {}\n", self);

    test_exit_status(self);
    test_wnohang(self);
    test_exec_pid_preserved(self);

    print("\n{} ({} failure{})\n", failures ? "FAILED" : "OK",
          failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
