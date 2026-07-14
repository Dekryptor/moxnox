#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern int optind;
extern int opterr;
extern int optopt;
extern char* optarg;

int getopt(int argc, char* const argv[], const char* optstring);

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
