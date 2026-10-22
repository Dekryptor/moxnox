// SPDX-License-Identifier: GPL-2.0-only
#include <setjmp.h>
#include <stdio.h>

#define GREEN "\e[32m"
#define BOLD_RED "\e[1;31m"
#define NORMAL "\e[0m"

static unsigned int tests_passed = 0;
static unsigned int tests_failed = 0;

static void check(bool condition, const char* msg) {
    if (condition) {
        printf("[" GREEN "PASS" NORMAL "] %s\n", msg);
        tests_passed++;
    } else {
        printf("[" BOLD_RED "FAIL" NORMAL "] %s\n", msg);
        tests_failed++;
    }
}

// Test 1: Return values

static void test_return_values() {
    printf("\nTest 1: Basic return values\n");

    jmp_buf buf;
    int val = setjmp(buf);
    if (val == 0)
        longjmp(buf, 42);
    check(val == 42, "setjmp returned 0 initially; longjmp(buf, 42) caused return of 42");

    jmp_buf buf2;
    int val2 = setjmp(buf2);
    if (val2 == 0)
        longjmp(buf2, 0);
    check(val2 == 1, "longjmp(buf, 0) caused setjmp to return 1");
}

// Test 2: Single-frame

static void test_single_frame() {
    printf("\nTest 2: Single-frame\n");

    jmp_buf buf;
    int val = setjmp(buf);
    if (val == 0)
        longjmp(buf, 1);
    check(val == 1, "single-frame longjmp resumed at setjmp with correct value");
}

// Test 3: Cross-frame

static jmp_buf cross_buf;

static void __attribute__((noinline)) cross_level_c() {
    longjmp(cross_buf, 77);
}

static void __attribute__((noinline)) cross_level_b() {
    cross_level_c();
}

static void __attribute__((noinline)) cross_level_a() {
    cross_level_b();
}

static void test_cross_frame() {
    printf("\nTest 3: Cross-frame\n");

    int val = setjmp(cross_buf);
    if (val == 0)
        cross_level_a();
    else
        check(val == 77, "cross-frame longjmp (3 levels deep) returned correct value");
}

int main() {
    printf("=== setjmp/longjmp test ===\n");

    test_return_values();
    test_single_frame();
    test_cross_frame();

    printf("\n%u passed, %u failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
