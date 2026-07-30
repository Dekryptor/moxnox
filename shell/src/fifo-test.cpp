/*
 * fifo-test - mkfifo and FIFO open/read/write semantics
 *
 * Covers creation (mkfifo, mkfifoat, S_ISFIFO, EEXIST), the open-side
 * rendezvous rules (nonblocking read-open succeeds with no writer,
 * nonblocking write-open fails ENXIO with no reader, make's jobserver
 * two-step idiom, blocking read-open waiting for a writer, O_RDWR
 * self-rendezvous), data flow (roundtrip, EAGAIN on empty nonblocking
 * read, the single shared queue feeding two readers), termination
 * semantics (EOF on last writer close, EOF not being sticky, EPIPE with
 * no readers), and poll() readiness.
 *
 * The creation tests come first, so they can pass before the VFS half of
 * the FIFO work is in place; everything from test_open_nonblock onwards
 * needs the pipe-object plumbing. A watchdog thread aborts the run after
 * 30 seconds, since rendezvous bugs present as hangs.
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <type_traits>
#include <utility>

import print;
import format;

using namespace std;

static const char* FIFO_PATH = "/tmp/fifo-test-fifo";
static const char* FIFO_PATH2 = "/tmp/fifo-test-fifo2";

static unsigned int failures = 0;

// Forwards to print(), keeping print's consteval format-string check at the call
// site (see spawn-test for the type_identity_t dance).
template<typename... Args>
static void check(bool ok, type_identity_t<checker<type_identity_t<Args>...>> fmt,
                  Args&&... args) {
    if (ok)
        print("\x1b[32mPASS\x1b[0m: ");
    else {
        print("\x1b[31mFAIL\x1b[0m: ");
        failures++;
    }
    print(fmt, std::forward<Args>(args)...);
    print("\n");
}

static void millisleep(unsigned int ms) {
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000;

    nanosleep(&ts, nullptr);
}

static void* watchdog(void*) {
    millisleep(30000);

    print("\x1b[31mTIMEOUT\x1b[0m: watchdog fired, a rendezvous is hanging\n");
    _exit(3);
}

// --- creation, no VFS support needed ---

static void test_create() {
    int r = mkfifo(FIFO_PATH, 0666);
    check(r == 0, "mkfifo returned {} (errno {})", r, r ? errno : 0);

    struct stat st;
    r = stat(FIFO_PATH, &st);
    check(r == 0, "stat on the fifo returned {}", r);
    if (r == 0)
        check(S_ISFIFO(st.st_mode), "st_mode reports S_IFIFO (mode {:o})", st.st_mode);

    errno = 0;
    r = mkfifo(FIFO_PATH, 0666);
    check(r == -1 && errno == EEXIST, "second mkfifo fails EEXIST (ret {}, errno {})",
          r, errno);

    r = mkfifoat(AT_FDCWD, FIFO_PATH2, 0666);
    check(r == 0, "mkfifoat(AT_FDCWD) returned {} (errno {})", r, r ? errno : 0);

    r = unlink(FIFO_PATH2);
    check(r == 0, "unlink of second fifo returned {}", r);
}

// --- open-side rendezvous ---

static void test_open_nonblock() {
    // nonblocking read-open must succeed with no writer present
    int rd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    check(rd >= 0, "O_RDONLY|O_NONBLOCK with no writers succeeds (fd {}, errno {})",
          rd, rd < 0 ? errno : 0);

    if (rd < 0)
        return;

    // make's jobserver idiom: blocking write-open sees our own read side
    int wr = open(FIFO_PATH, O_WRONLY);
    check(wr >= 0, "O_WRONLY with a reader present succeeds (fd {}, errno {})",
          wr, wr < 0 ? errno : 0);

    // empty fifo, nonblocking read
    char c;
    errno = 0;
    auto n = read(rd, &c, 1);
    check(n == -1 && errno == EAGAIN, "nonblocking read on empty fifo gives EAGAIN (ret {}, errno {})",
          (int)n, errno);

    // roundtrip
    if (wr >= 0) {
        n = write(wr, "x", 1);
        check(n == 1, "write of one byte returned {}", (int)n);

        n = read(rd, &c, 1);
        check(n == 1 && c == 'x', "read got the byte back (ret {}, c '{}')", (int)n, c);

        close(wr);
    }

    close(rd);

    // with everything closed again, nonblocking write-open has no reader
    errno = 0;
    wr = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
    check(wr == -1 && errno == ENXIO, "O_WRONLY|O_NONBLOCK with no readers fails ENXIO (ret {}, errno {})",
          wr, errno);

    if (wr >= 0)
        close(wr);
}

static bool blocking_open_returned = false;
static int blocking_open_fd = -1;

static void* blocking_reader(void*) {
    blocking_open_fd = open(FIFO_PATH, O_RDONLY);
    blocking_open_returned = true;

    return nullptr;
}

static void test_open_rendezvous() {
    pthread_t t;

    blocking_open_returned = false;
    blocking_open_fd = -1;

    pthread_create(&t, nullptr, blocking_reader, nullptr);

    millisleep(100);

    check(!blocking_open_returned, "blocking read-open does not return before a writer appears");

    int wr = open(FIFO_PATH, O_WRONLY);
    check(wr >= 0, "blocking write-open completes against the waiting reader (fd {}, errno {})",
          wr, wr < 0 ? errno : 0);

    pthread_join(t, nullptr);
    check(blocking_open_returned && blocking_open_fd >= 0,
          "blocking read-open completed once the writer arrived (fd {})", blocking_open_fd);

    if (wr >= 0 && blocking_open_fd >= 0) {
        char c;

        auto n = write(wr, "y", 1);
        check(n == 1, "write across the rendezvous returned {}", (int)n);

        n = read(blocking_open_fd, &c, 1);
        check(n == 1 && c == 'y', "read across the rendezvous got the byte (ret {}, c '{}')",
              (int)n, c);
    }

    if (wr >= 0)
        close(wr);
    if (blocking_open_fd >= 0)
        close(blocking_open_fd);
}

// --- one shared queue, multiple readers ---

static char reader_got[2];

static void* one_byte_reader(void* arg) {
    auto idx = (uintptr_t)arg;
    int fd = open(FIFO_PATH, O_RDONLY);

    if (fd < 0) {
        reader_got[idx] = '?';
        return nullptr;
    }

    char c = '?';
    auto n = read(fd, &c, 1);

    reader_got[idx] = n == 1 ? c : '?';

    close(fd);

    return nullptr;
}

static void test_shared_queue() {
    pthread_t t1, t2;

    // hold a write side so the readers' opens succeed immediately
    int rd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    int wr = open(FIFO_PATH, O_WRONLY);

    check(rd >= 0 && wr >= 0, "queue-test setup fds (rd {}, wr {})", rd, wr);
    if (rd < 0 || wr < 0)
        return;

    close(rd); // the reader threads provide the read sides from here

    reader_got[0] = reader_got[1] = 0;

    pthread_create(&t1, nullptr, one_byte_reader, (void*)0);
    pthread_create(&t2, nullptr, one_byte_reader, (void*)1);

    millisleep(100); // let both block in read

    auto n = write(wr, "AB", 2);
    check(n == 2, "write of two bytes returned {}", (int)n);

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);

    bool each_got_one = (reader_got[0] == 'A' && reader_got[1] == 'B') ||
                        (reader_got[0] == 'B' && reader_got[1] == 'A');

    check(each_got_one, "two readers each consumed exactly one byte ('{}', '{}')",
          reader_got[0], reader_got[1]);

    close(wr);
}

// --- termination semantics ---

static void test_eof_and_epipe() {
    int rd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    int wr = open(FIFO_PATH, O_WRONLY);

    check(rd >= 0 && wr >= 0, "eof-test setup fds (rd {}, wr {})", rd, wr);
    if (rd < 0 || wr < 0)
        return;

    write(wr, "z", 1);
    close(wr);

    char c;
    auto n = read(rd, &c, 1);
    check(n == 1 && c == 'z', "buffered data still readable after writer close (ret {})", (int)n);

    n = read(rd, &c, 1);
    check(n == 0, "read gives EOF once drained with no writers (ret {}, errno {})",
          (int)n, n < 0 ? errno : 0);

    // EOF is not sticky: a new writer revives the stream
    wr = open(FIFO_PATH, O_WRONLY);
    check(wr >= 0, "write-open after EOF succeeds (fd {}, errno {})", wr, wr < 0 ? errno : 0);

    if (wr >= 0) {
        write(wr, "w", 1);

        n = read(rd, &c, 1);
        check(n == 1 && c == 'w', "read succeeds again after a new writer appears (ret {})",
              (int)n);
    }

    // EPIPE: writer left with no readers
    close(rd);

    errno = 0;
    n = write(wr, "n", 1);
    check(n == -1 && errno == EPIPE, "write with no readers fails EPIPE (ret {}, errno {})",
          (int)n, errno);

    close(wr);
}

// --- O_RDWR self-rendezvous ---

static void test_rdwr() {
    int fd = open(FIFO_PATH, O_RDWR);
    check(fd >= 0, "O_RDWR opens without blocking or a counterpart (fd {}, errno {})",
          fd, fd < 0 ? errno : 0);

    if (fd < 0)
        return;

    char c;
    auto n = write(fd, "q", 1);
    check(n == 1, "write through O_RDWR fd returned {}", (int)n);

    n = read(fd, &c, 1);
    check(n == 1 && c == 'q', "read back through the same fd (ret {}, c '{}')", (int)n, c);

    close(fd);
}

// --- poll readiness ---

static void test_poll() {
    int rd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    int wr = open(FIFO_PATH, O_WRONLY);

    check(rd >= 0 && wr >= 0, "poll-test setup fds (rd {}, wr {})", rd, wr);
    if (rd < 0 || wr < 0)
        return;

    struct pollfd pfd;

    pfd.fd = rd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int r = poll(&pfd, 1, 0);
    check(r == 0, "empty fifo is not readable (poll ret {}, revents {:x})", r, pfd.revents);

    write(wr, "p", 1);

    pfd.revents = 0;
    r = poll(&pfd, 1, 1000);
    check(r == 1 && (pfd.revents & POLLIN),
          "fifo becomes readable after a write (poll ret {}, revents {:x})", r, pfd.revents);

    close(rd);
    close(wr);
}

int main() {
    pthread_t wd;

    signal(SIGPIPE, SIG_IGN);

    unlink(FIFO_PATH);
    unlink(FIFO_PATH2);

    pthread_create(&wd, nullptr, watchdog, nullptr);

    test_create();
    test_open_nonblock();
    test_open_rendezvous();
    test_shared_queue();
    test_eof_and_epipe();
    test_rdwr();
    test_poll();

    unlink(FIFO_PATH);

    print("\n{} ({} failure{})\n", failures ? "FAILED" : "OK",
          failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
