#pragma once

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

struct option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

enum {
    no_argument = 0,
    required_argument,
    optional_argument
};

int getopt_long(int argc, char* const argv[], const char* optstring,
                const struct option* long_options, int* idx);
int getopt_long_only(int argc, char* const argv[], const char* optstring,
                     const struct option* long_options, int* idx);

#ifdef __cplusplus
}
#endif
