/*
 * spawn-test - process primitives: posix_spawn, waitpid, exec, fd inheritance.
 *
 * Re-spawns itself in a "child mode" to create children with known behaviour,
 * then exercises the wait path: exit-status plumbing, pid matching, WNOHANG
 * before/after exit, and ECHILD once a child has been reaped (M0).
 *
 * M1 adds exec: a spawned child execve()s itself into a second mode, and we
 * check the pid is preserved across the exec - the original pid reaps the
 * exec'd image, which still sees its own pid and its parent unchanged.
 *
 * M2 adds fd inheritance: a pipe's read end is handed to a child by fd number,
 * and we check it is inherited across posix_spawn and across exec, and that
 * setting FD_CLOEXEC suppresses it in both cases.
 *
 * M3 exercises the exec* family (execv/execve/execl/execle/execvp/execlp): each
 * variant execs into a probe that checks argv marshalling, the environment
 * (inherited for the non-e forms, explicit for the e forms), and that an
 * argument containing a space survives the command line. It also checks that a
 * PATH search for a missing name fails with ENOENT and that a name with a slash
 * bypasses the search.
 *
 * M4 covers waitpid edge cases: waitpid(-1) reaping any of several children and
 * then ECHILD once none remain, ECHILD for a wait on a pid that is not our
 * child, and a child killed by a signal being reported via WIFSIGNALED/WTERMSIG.
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
#include <fcntl.h>
#include <signal.h>
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

// M2 fd-inheritance markers. The parent writes fd_known_byte into a pipe; a child
// that inherits the read end exits with that byte, while a child that does not
// (FD_CLOEXEC, or no inheritance) sees the read fail and exits fd_read_closed_marker.
static constexpr int fd_known_byte = 0x42;
static constexpr int fd_read_closed_marker = 200;

// M3 marker: do_m3_notfound returns this when execvp of a missing name fails
// with ENOENT, as POSIX requires.
static constexpr int m3_notfound_ok = 50;

// M4 marker: do_raise returns this if raise() came back, i.e. the signal did not
// terminate the child.
static constexpr int raise_returned_marker = 77;

// Forwards to print(). The format string is taken as print's own consteval
// `checker` type (wrapped in type_identity_t so Args deduce from the trailing
// arguments, not the format), so the compile-time format check still happens at
// the call site; the constructed checker is then handed straight to print().
template<typename... Args>
static void check(bool ok, type_identity_t<checker<type_identity_t<Args>...>> fmt,
                  Args&&... args) {
    if (ok)
        print("\x1b[32mPASS\x1b[0m: ");
    else {
        print("\x1b[31mFAIL\x1b[0m: ");
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

// Child mode: read one byte from the given fd. Exit code is the byte read, or
// fd_read_closed_marker if the fd is not open (read failed) - which is what a
// non-inherited / closed-on-exec fd should look like.
static int do_fd_read(const char* fd_str) {
    int fd = atoi(fd_str);
    unsigned char c = 0;

    if (read(fd, &c, 1) != 1)
        return fd_read_closed_marker;

    return c;
}

// Child mode: optionally mark the fd close-on-exec, then exec into --fd-read so
// the read happens in a freshly exec'd image. This checks the fd survives (or is
// correctly dropped) across exec, on top of the spawn that got us here.
static int do_exec_read(const char* fd_str, const char* set_cloexec) {
    const char* self = self_path();
    if (!self)
        return exec_failed_marker;

    if (atoi(set_cloexec))
        fcntl(atoi(fd_str), F_SETFD, FD_CLOEXEC);

    char* eargv[] = { (char*)self, (char*)"--fd-read", (char*)fd_str, nullptr };
    char* env[] = { (char*)"PATH=/bin", nullptr };

    execve(self, eargv, env);

    return exec_failed_marker;
}

// Set up a pipe, write the known byte into it, and spawn the child in `mode` with
// the read-end fd number (plus an optional extra arg). Returns the child's exit
// code, or a negative value if the harness itself (pipe/spawn) failed. When
// set_cloexec is true, FD_CLOEXEC is set on the read end before spawning.
static int run_fd_scenario(const char* self, const char* mode,
                           const char* extra, bool set_cloexec) {
    int pf[2];
    if (pipe(pf) != 0)
        return -1;

    if (set_cloexec)
        fcntl(pf[0], F_SETFD, FD_CLOEXEC);

    unsigned char b = fd_known_byte;
    write(pf[1], &b, 1);

    char fd_str[16];
    snprintf(fd_str, sizeof(fd_str), "%d", pf[0]);

    char* cargv[5];
    cargv[0] = (char*)self;
    cargv[1] = (char*)mode;
    cargv[2] = fd_str;
    if (extra) {
        cargv[3] = (char*)extra;
        cargv[4] = nullptr;
    } else {
        cargv[3] = nullptr;
    }

    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    if (err != 0) {
        close(pf[0]);
        close(pf[1]);
        return -1000 - err;
    }

    int status = 0;
    waitpid(pid, &status, 0);

    close(pf[0]);
    close(pf[1]);

    if (!WIFEXITED(status))
        return -2000;

    return WEXITSTATUS(status);
}

// A plain (non-CLOEXEC) read fd must be inherited by a posix_spawn'd child.
static void test_fd_inherit_spawn(const char* self) {
    int code = run_fd_scenario(self, "--fd-read", nullptr, false);
    check(code == fd_known_byte,
          "fd inherited across posix_spawn (child read {}, expected {})",
          code, fd_known_byte);
}

// FD_CLOEXEC on the read fd must stop it being inherited by the spawned child.
static void test_fd_cloexec_spawn(const char* self) {
    int code = run_fd_scenario(self, "--fd-read", nullptr, true);
    check(code == fd_read_closed_marker,
          "FD_CLOEXEC fd not inherited across posix_spawn (child got {}, expected {})",
          code, fd_read_closed_marker);
}

// A plain read fd must survive both the spawn and a subsequent exec.
static void test_fd_inherit_exec(const char* self) {
    int code = run_fd_scenario(self, "--fd-exec-read", "0", false);
    check(code == fd_known_byte,
          "fd survives posix_spawn then exec (child read {}, expected {})",
          code, fd_known_byte);
}

// An fd inherited by the child but marked FD_CLOEXEC before it execs must be
// dropped at that exec.
static void test_fd_cloexec_exec(const char* self) {
    int code = run_fd_scenario(self, "--fd-exec-read", "1", false);
    check(code == fd_read_closed_marker,
          "FD_CLOEXEC fd dropped across exec (child got {}, expected {})",
          code, fd_read_closed_marker);
}

static const char* base_name(const char* path) {
    const char* base = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    return base;
}

// Child mode: the image every M3 variant execs into. argv is expected to be
//   <name> --m3-probe <env-value> ALPHA GAMMA "two words"
// Reports through the exit code: bit 0 = the space-free args arrived intact,
// bit 1 = M3VAR matches the expected value, bit 2 = the arg containing a space
// survived command-line quoting. The spacey arg is last so a botched quote
// (which splits it) shifts only the tail and leaves the earlier checks valid.
static int do_m3_probe(int argc, char** argv) {
    if (argc < 6)
        return 0;

    int result = 0;

    if (!strcmp(argv[3], "ALPHA") && !strcmp(argv[4], "GAMMA"))
        result |= 1;

    const char* env = getenv("M3VAR");
    if (env && !strcmp(env, argv[2]))
        result |= 2;

    if (argc == 6 && !strcmp(argv[5], "two words"))
        result |= 4;

    return result;
}

// Child mode: perform one exec* variant, landing in --m3-probe. The non-e forms
// inherit our environment (we put M3VAR there with setenv), so the probe expects
// "envval"; the e forms pass an explicit environment with a different value, so
// the probe expects "explicitval" - which also proves the explicit env won out
// over the inherited one. The p forms are given the bare image name so they
// exercise the PATH search.
static int do_m3_exec(const char* variant) {
    const char* self = self_path();
    if (!self)
        return exec_failed_marker;

    setenv("M3VAR", "envval", 1);

    char* explicit_env[] = { (char*)"M3VAR=explicitval", (char*)"PATH=/bin",
                             nullptr };

    auto base = base_name(self);

    if (!strcmp(variant, "v")) {
        char* a[] = { (char*)self, (char*)"--m3-probe", (char*)"envval",
                      (char*)"ALPHA", (char*)"GAMMA", (char*)"two words",
                      nullptr };
        execv(self, a);
    } else if (!strcmp(variant, "ve")) {
        char* a[] = { (char*)self, (char*)"--m3-probe", (char*)"explicitval",
                      (char*)"ALPHA", (char*)"GAMMA", (char*)"two words",
                      nullptr };
        execve(self, a, explicit_env);
    } else if (!strcmp(variant, "l")) {
        execl(self, self, "--m3-probe", "envval",
              "ALPHA", "GAMMA", "two words", (char*)nullptr);
    } else if (!strcmp(variant, "le")) {
        execle(self, self, "--m3-probe", "explicitval",
               "ALPHA", "GAMMA", "two words", (char*)nullptr, explicit_env);
    } else if (!strcmp(variant, "vp")) {
        char* a[] = { (char*)base, (char*)"--m3-probe", (char*)"envval",
                      (char*)"ALPHA", (char*)"GAMMA", (char*)"two words",
                      nullptr };
        execvp(base, a);
    } else if (!strcmp(variant, "lp")) {
        execlp(base, base, "--m3-probe", "envval",
               "ALPHA", "GAMMA", "two words", (char*)nullptr);
    }

    // A successful exec never returns.
    return exec_failed_marker;
}

// Child mode: execvp a name that is not on PATH; it must fail with ENOENT rather
// than succeeding or failing some other way.
static int do_m3_notfound() {
    char* a[] = { (char*)"no-such-program-xyzzy", (char*)"--m3-probe", nullptr };

    errno = 0;
    execvp("no-such-program-xyzzy", a);

    // execvp returned, so it failed - as it should.
    return errno == ENOENT ? m3_notfound_ok : 0;
}

// Child mode: execvp a name with a slash while PATH is deliberately useless.
// Because the name contains a slash the search must be skipped and the name used
// directly, so the exec should still succeed. The path is built as /bin/<self>
// to guarantee a slash regardless of how the image path is spelt.
static int do_m3_slashed() {
    const char* self = self_path();
    if (!self)
        return exec_failed_marker;

    char path[512];
    snprintf(path, sizeof(path), "/bin/%s", base_name(self));

    setenv("M3VAR", "envval", 1);
    setenv("PATH", "/no-such-dir", 1);

    char* a[] = { path, (char*)"--m3-probe", (char*)"envval",
                  (char*)"ALPHA", (char*)"GAMMA", (char*)"two words",
                  nullptr };
    execvp(path, a);

    return exec_failed_marker;
}

// Run one exec* variant: spawn a child that performs it, landing in the probe,
// and check argv marshalling, environment, and space-quoting via the exit code.
static void test_m3_variant(const char* self, const char* variant) {
    char* cargv[] = { (char*)self, (char*)"--m3-exec", (char*)variant, nullptr };
    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    check(err == 0, "posix_spawn (exec{}) returned {}", variant, err);
    if (err != 0)
        return;

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    check(w == pid, "exec{}: waitpid returned child", variant);
    check(WIFEXITED(status), "exec{}: child exited normally", variant);
    if (w != pid || !WIFEXITED(status))
        return;

    int code = WEXITSTATUS(status);
    if (code == exec_failed_marker) {
        check(false, "exec{} failed (exec returned)", variant);
        return;
    }

    check((code & 1) != 0, "exec{}: space-free args passed through", variant);
    check((code & 2) != 0, "exec{}: environment passed through", variant);
    check((code & 4) != 0, "exec{}: arg with a space survived quoting", variant);
}

static void test_m3_notfound(const char* self) {
    char* cargv[] = { (char*)self, (char*)"--m3-notfound", nullptr };
    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    check(err == 0, "posix_spawn (m3-notfound) returned {}", err);
    if (err != 0)
        return;

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    check(w == pid && WIFEXITED(status) && WEXITSTATUS(status) == m3_notfound_ok,
          "execvp of missing name fails with ENOENT (status {})",
          WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

static void test_m3_slashed(const char* self) {
    char* cargv[] = { (char*)self, (char*)"--m3-slashed", nullptr };
    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    check(err == 0, "posix_spawn (m3-slashed) returned {}", err);
    if (err != 0)
        return;

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    check(w == pid && WIFEXITED(status) && WEXITSTATUS(status) != exec_failed_marker
              && (WEXITSTATUS(status) & 1),
          "execvp with a slashed path skips PATH and runs (status {})",
          WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

// Child mode: raise a signal on ourselves. A default-fatal signal should
// terminate us; if raise() returns, the signal did not kill us, so report that.
static int do_raise(const char* sig_str) {
    raise(atoi(sig_str));
    return raise_returned_marker;
}

// waitpid(-1) must reap any child: spawn several with distinct exit codes, reap
// them all without naming pids and check each status, then ECHILD once none
// remain.
static void test_waitpid_any(const char* self) {
    const int codes[] = { 11, 22, 33 };
    const unsigned int n = 3;

    pid_t pids[n];
    int exp_code[n];
    bool reaped[n];
    unsigned int spawned = 0;

    for (unsigned int i = 0; i < n; i++) {
        char code_str[16];
        snprintf(code_str, sizeof(code_str), "%d", codes[i]);

        char* cargv[] = { (char*)self, (char*)"--exit", code_str, nullptr };
        pid_t pid = -1;
        if (spawn_self(self, cargv, &pid) != 0)
            continue;

        pids[spawned] = pid;
        exp_code[spawned] = codes[i];
        reaped[spawned] = false;
        spawned++;
    }

    check(spawned == n, "spawned {} children for waitpid(-1) (got {})", n, spawned);

    unsigned int got = 0;
    bool all_match = true;

    for (unsigned int i = 0; i < spawned; i++) {
        int status = 0;
        pid_t w = waitpid(-1, &status, 0);

        bool found = false;
        for (unsigned int k = 0; k < spawned; k++) {
            if (pids[k] == w && !reaped[k]) {
                reaped[k] = true;
                found = true;
                if (!WIFEXITED(status) || WEXITSTATUS(status) != exp_code[k])
                    all_match = false;
                break;
            }
        }

        if (found)
            got++;
        else
            all_match = false;
    }

    check(got == spawned, "waitpid(-1) reaped all {} children (got {})", spawned, got);
    check(all_match, "each child's exit status came back correctly via waitpid(-1)");

    errno = 0;
    int status = 0;
    pid_t w = waitpid(-1, &status, 0);
    check(w == -1 && errno == ECHILD,
          "waitpid(-1) with no children gives ECHILD (got {}, errno {})",
          (int)w, errno);
}

// waitpid on a pid that exists but is not our child must give ECHILD. We use our
// own pid (a process is not its own child); WNOHANG keeps this from blocking if
// the not-our-child check isn't in place yet.
static void test_waitpid_nonchild() {
    errno = 0;
    int status = 0;
    pid_t w = waitpid(getpid(), &status, WNOHANG);
    check(w == -1 && errno == ECHILD,
          "waitpid on a non-child pid gives ECHILD (got {}, errno {})", (int)w, errno);
}

// A child killed by a signal must be reported via WIFSIGNALED/WTERMSIG, not as a
// normal exit.
static void test_waitpid_signal(const char* self) {
    char sig_str[16];
    snprintf(sig_str, sizeof(sig_str), "%d", SIGTERM);

    char* cargv[] = { (char*)self, (char*)"--raise", sig_str, nullptr };
    pid_t pid = -1;
    int err = spawn_self(self, cargv, &pid);
    check(err == 0, "posix_spawn (raise) returned {}", err);
    if (err != 0)
        return;

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    check(w == pid, "waitpid (raise) returned child (got {})", (int)w);
    check(WIFSIGNALED(status), "child reported as killed by a signal");
    check(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM,
          "WTERMSIG is SIGTERM (got {})", WIFSIGNALED(status) ? WTERMSIG(status) : -1);
    check(!WIFEXITED(status), "signal death not reported as a normal exit");
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

    if (argc >= 3 && !strcmp(argv[1], "--fd-read"))
        return do_fd_read(argv[2]);

    if (argc >= 4 && !strcmp(argv[1], "--fd-exec-read"))
        return do_exec_read(argv[2], argv[3]);

    if (argc >= 3 && !strcmp(argv[1], "--m3-exec"))
        return do_m3_exec(argv[2]);

    if (argc >= 2 && !strcmp(argv[1], "--m3-probe"))
        return do_m3_probe(argc, argv);

    if (argc >= 2 && !strcmp(argv[1], "--m3-notfound"))
        return do_m3_notfound();

    if (argc >= 2 && !strcmp(argv[1], "--m3-slashed"))
        return do_m3_slashed();

    if (argc >= 3 && !strcmp(argv[1], "--raise"))
        return do_raise(argv[2]);

    // --- Driver ---
    const char* self = self_path();
    if (!self) {
        print("\x1b[31mFAIL\x1b[0m: could not determine own path\n");
        return 1;
    }
    print("self: {}\n", self);

    test_exit_status(self);
    test_wnohang(self);
    test_exec_pid_preserved(self);
    test_fd_inherit_spawn(self);
    test_fd_cloexec_spawn(self);
    test_fd_inherit_exec(self);
    test_fd_cloexec_exec(self);
    test_m3_variant(self, "v");
    test_m3_variant(self, "ve");
    test_m3_variant(self, "l");
    test_m3_variant(self, "le");
    test_m3_variant(self, "vp");
    test_m3_variant(self, "lp");
    test_m3_notfound(self);
    test_m3_slashed(self);
    test_waitpid_any(self);
    test_waitpid_nonchild();
    test_waitpid_signal(self);

    print("\n{} ({} failure{})\n", failures ? "FAILED" : "OK",
          failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
