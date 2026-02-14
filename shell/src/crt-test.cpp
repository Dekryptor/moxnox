#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GREEN "\e[32m"
#define BOLD_RED "\e[1;31m"
#define NORMAL "\e[0m"

// Global variables (tests .data section)
int global_int = 42;
char global_string[] = "Global data works";

// BSS (uninitialized data)
int bss_array[100];

// Global constructor (C++)
struct GlobalConstructorTest {
    GlobalConstructorTest() {
        printf("[" GREEN "PASS" NORMAL "] Global constructor called\n");
    }
    ~GlobalConstructorTest() {
        printf("[" GREEN "PASS" NORMAL "] Global destructor called\n");
    }
} global_obj;

// atexit handler
void cleanup_handler() {
    printf("[" GREEN "PASS" NORMAL "] atexit handler called\n");
}

int main(int argc, char** argv, char** envp) {
    printf("=== CRT Basic Test ===\n\n");

    // Test 1: main() called with arguments
    printf("[TEST] Command line arguments\n");
    printf("  argc = %d\n", argc);
    if (argc > 0) {
        printf("  argv[0] = %s\n", argv[0]);
        printf("[" GREEN "PASS" NORMAL "] Command line parsing works\n");
    } else {
        printf("[" BOLD_RED "FAIL" NORMAL "] argc is 0\n");
        return 1;
    }

    // Test 2: Environment variables
    printf("\n[TEST] Environment variables\n");
    if (envp && envp[0]) {
        printf("  First env var: %s\n", envp[0]);
        printf("[" GREEN "PASS" NORMAL "] Environment accessible\n");
    } else {
        printf("[WARN] No environment variables\n");
    }

    // Test 3: Global data
    printf("\n[TEST] Global data\n");
    if (global_int == 42) {
        printf("  global_int = %d\n", global_int);
        printf("[" GREEN "PASS" NORMAL "] .data section initialized\n");
    } else {
        printf("[" BOLD_RED "FAIL" NORMAL "] Global int wrong: %d\n", global_int);
    }

    // Test 4: BSS zeroing
    printf("\n[TEST] BSS section\n");
    int all_zero = 1;
    for (int i = 0; i < 100; i++) {
        if (bss_array[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    if (all_zero) {
        printf("[" GREEN "PASS" NORMAL "] BSS zeroed correctly\n");
    } else {
        printf("[" BOLD_RED "FAIL" NORMAL "] BSS not zeroed\n");
    }

    // Test 5: atexit
    printf("\n[TEST] atexit registration\n");
    if (atexit(cleanup_handler) == 0) {
        printf("[" GREEN "PASS" NORMAL "] atexit registered\n");
    } else {
        printf("[" BOLD_RED "FAIL" NORMAL "] atexit failed\n");
    }

    printf("\n=== Basic tests complete ===\n");
    return 0;
}
