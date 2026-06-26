/*
 * pthread-test - basic pthreads coverage
 *
 * Covers thread-specific data (key create/get/set, and destructors run at thread
 * exit), thread create/join with return-value propagation, mutex mutual
 * exclusion, and condition-variable wake. The tests run in an order where the
 * TSD-without-threads case comes first, so it can pass before thread creation is
 * in place.
 */

#include <pthread.h>
#include <stdint.h>
#include <type_traits>
#include <utility>

import print;
import format;

using namespace std;

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

// --- thread-specific data, no threads needed ---

static void test_key_create() {
    pthread_key_t key;
    int r = pthread_key_create(&key, nullptr);
    check(r == 0, "pthread_key_create returned {}", r);
    if (r != 0)
        return;

    check(pthread_getspecific(key) == nullptr, "a fresh key reads back NULL");

    pthread_setspecific(key, (void*)(uintptr_t)0xabc);
    check(pthread_getspecific(key) == (void*)(uintptr_t)0xabc,
          "setspecific/getspecific round-trips");

    pthread_setspecific(key, nullptr);
    check(pthread_getspecific(key) == nullptr, "setspecific(NULL) clears the value");

    pthread_key_delete(key);
}

// --- create / join ---

static void* return_plus_one(void* arg) {
    return (void*)((uintptr_t)arg + 1);
}

static void test_create_join() {
    pthread_t t;
    int r = pthread_create(&t, nullptr, return_plus_one, (void*)(uintptr_t)41);
    check(r == 0, "pthread_create returned {}", r);
    if (r != 0)
        return;

    void* ret = nullptr;
    r = pthread_join(t, &ret);
    check(r == 0, "pthread_join returned {}", r);
    check((uintptr_t)ret == 42, "thread return value propagates through join (got {})",
          (uintptr_t)ret);
}

// --- per-thread independence ---

static pthread_key_t shared_key;

static void* per_thread_fn(void*) {
    uintptr_t res = 0;

    if (pthread_getspecific(shared_key) == nullptr)
        res |= 1; // starts NULL in this thread

    pthread_setspecific(shared_key, (void*)(uintptr_t)0xbbb);

    if (pthread_getspecific(shared_key) == (void*)(uintptr_t)0xbbb)
        res |= 2;

    return (void*)res;
}

static void test_key_per_thread() {
    int r = pthread_key_create(&shared_key, nullptr);
    check(r == 0, "pthread_key_create (per-thread) returned {}", r);
    if (r != 0)
        return;

    pthread_setspecific(shared_key, (void*)(uintptr_t)0xaaa);

    pthread_t t;
    if (pthread_create(&t, nullptr, per_thread_fn, nullptr) == 0) {
        void* ret = nullptr;
        pthread_join(t, &ret);
        check(((uintptr_t)ret & 1) != 0, "key starts NULL in a new thread");
        check(((uintptr_t)ret & 2) != 0, "setspecific works in a new thread");
    }

    check(pthread_getspecific(shared_key) == (void*)(uintptr_t)0xaaa,
          "our key value is unchanged by the other thread");

    pthread_key_delete(shared_key);
}

// --- destructor runs on thread exit ---

static pthread_key_t dtor_key;
static int dtor_value = -1;

static void record_dtor(void* v) {
    dtor_value = (int)(uintptr_t)v;
}

static void* set_then_exit(void*) {
    pthread_setspecific(dtor_key, (void*)(uintptr_t)123);
    return nullptr;
}

static void test_key_destructor() {
    dtor_value = -1;

    int r = pthread_key_create(&dtor_key, record_dtor);
    check(r == 0, "pthread_key_create (destructor) returned {}", r);
    if (r != 0)
        return;

    pthread_t t;
    if (pthread_create(&t, nullptr, set_then_exit, nullptr) == 0) {
        pthread_join(t, nullptr);
        // join returns only once the thread has terminated, by which point its
        // key destructors must already have run.
        check(dtor_value == 123,
              "key destructor ran on thread exit with the value (got {})", dtor_value);
    }

    pthread_key_delete(dtor_key);
}

// --- destructor re-runs when it re-sets the value ---

static pthread_key_t rerun_key;
static int rerun_count = 0;

static void rerun_dtor(void* v) {
    rerun_count++;

    auto n = (uintptr_t)v;
    if (n > 1)
        pthread_setspecific(rerun_key, (void*)(n - 1));
}

static void* arm_rerun(void*) {
    pthread_setspecific(rerun_key, (void*)(uintptr_t)3);
    return nullptr;
}

static void test_key_destructor_rerun() {
    rerun_count = 0;

    int r = pthread_key_create(&rerun_key, rerun_dtor);
    check(r == 0, "pthread_key_create (rerun) returned {}", r);
    if (r != 0)
        return;

    pthread_t t;
    if (pthread_create(&t, nullptr, arm_rerun, nullptr) == 0) {
        pthread_join(t, nullptr);
        check(rerun_count == 3,
              "destructor re-runs while it re-sets the value (ran {}, expected 3)",
              rerun_count);
    }

    pthread_key_delete(rerun_key);
}

// --- mutex mutual exclusion ---

static pthread_mutex_t test_mtx;
static unsigned long mutex_counter = 0;
static constexpr unsigned int mutex_incs = 50000;

static void* mutex_increment(void*) {
    for (unsigned int i = 0; i < mutex_incs; i++) {
        pthread_mutex_lock(&test_mtx);
        mutex_counter++;
        pthread_mutex_unlock(&test_mtx);
    }
    return nullptr;
}

static void test_mutex() {
    int r = pthread_mutex_init(&test_mtx, nullptr);
    check(r == 0, "pthread_mutex_init returned {}", r);
    if (r != 0)
        return;

    mutex_counter = 0;

    static const unsigned int n = 4;
    pthread_t t[n];
    unsigned int started = 0;

    for (unsigned int i = 0; i < n; i++) {
        if (pthread_create(&t[i], nullptr, mutex_increment, nullptr) != 0)
            break;
        started++;
    }
    check(started == n, "started all {} mutex threads (got {})", n, started);

    for (unsigned int i = 0; i < started; i++) {
        pthread_join(t[i], nullptr);
    }

    check(mutex_counter == (unsigned long)started * mutex_incs,
          "mutex serialised increments (got {}, expected {})",
          mutex_counter, (unsigned long)started * mutex_incs);

    pthread_mutex_destroy(&test_mtx);
}

// --- condition variable wake ---

static pthread_mutex_t cond_mtx;
static pthread_cond_t cond_cv;
static bool cond_ready = false;
static bool cond_woke = false;

static void* cond_waiter(void*) {
    pthread_mutex_lock(&cond_mtx);

    while (!cond_ready) {
        pthread_cond_wait(&cond_cv, &cond_mtx);
    }

    cond_woke = true;
    pthread_mutex_unlock(&cond_mtx);
    return nullptr;
}

static void test_cond() {
    pthread_mutex_init(&cond_mtx, nullptr);
    pthread_cond_init(&cond_cv, nullptr);
    cond_ready = false;
    cond_woke = false;

    pthread_t t;
    if (pthread_create(&t, nullptr, cond_waiter, nullptr) != 0) {
        check(false, "cond: could not create waiter thread");
        pthread_cond_destroy(&cond_cv);
        pthread_mutex_destroy(&cond_mtx);
        return;
    }

    pthread_mutex_lock(&cond_mtx);
    cond_ready = true;
    pthread_cond_signal(&cond_cv);
    pthread_mutex_unlock(&cond_mtx);

    pthread_join(t, nullptr);
    check(cond_woke, "cond_wait woke after signal");

    pthread_cond_destroy(&cond_cv);
    pthread_mutex_destroy(&cond_mtx);
}

int main() {
    test_key_create();
    test_create_join();
    test_key_per_thread();
    test_key_destructor();
    test_key_destructor_rerun();
    test_mutex();
    test_cond();

    print("\n{} ({} failure{})\n", failures ? "FAILED" : "OK",
          failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
