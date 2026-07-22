#pragma once

/* TRANSITIONAL: the MS __int64 keyword, defined away to the real
 * type as mingw's headers always did for GCC and Clang. */
#ifndef __int64
#define __int64 long long
#endif

/* TRANSITIONAL: the prebuilt gcc runtime's win32 gthr layer needs
 * this to enable condition variables; mingw's _mingw.h used to
 * default it. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0603
#endif

#include <stddef.h>

#ifndef _INC_STDLIB // FIXME
#define _INC_STDLIB

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define RAND_MAX 0x7fff

#define _MAX_PATH 260
#define _MAX_FNAME 256

#ifndef _DIV_T_DEFINED // FIXME
#define _DIV_T_DEFINED

typedef struct _div_t {
    int quot;
    int rem;
} div_t;

typedef struct _ldiv_t {
    long quot;
    long rem;
} ldiv_t;

#endif

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

#if defined(__cplusplus) && __cplusplus >= 201103L
#define __moxnox_noexcept noexcept
#elif defined(__cplusplus)
#define __moxnox_noexcept throw()
#else
#define __moxnox_noexcept
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _CRT_ERRNO_DEFINED // FIXME
#define _CRT_ERRNO_DEFINED

int* _errno(void);
#define errno (*_errno())

#endif

int ___mb_cur_max_func(void);

#define MB_CUR_MAX ___mb_cur_max_func()

double atof(const char* nptr);
int atoi(const char* nptr);
long atol(const char* nptr);
long long atoll(const char* nptr);
double strtod(const char* nptr, char** endptr);
float strtof(const char* nptr, char** endptr);
long double strtold(const char* nptr, char** endptr);
long strtol(const char* nptr, char** endptr, int base);
long long strtoll(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
unsigned long long strtoull(const char* nptr, char** endptr, int base);
int rand(void);
void srand(unsigned int seed);
int rand_s(unsigned int* value);
void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void* _aligned_malloc(size_t size, size_t alignment);
void _aligned_free(void* ptr);
void free(void* ptr);

__attribute__((noreturn)) void abort(void) __moxnox_noexcept;
int atexit(void (*func)(void)) __moxnox_noexcept;
__attribute__((noreturn)) void exit(int status) __moxnox_noexcept;
__attribute__((noreturn)) void _exit(int status) __moxnox_noexcept;
__attribute__((noreturn)) void _Exit(int status) __moxnox_noexcept;

typedef int (*_onexit_t)(void);
_onexit_t _onexit(_onexit_t func);

extern char*** __p__environ(void);

#define _environ (*__p__environ())
#define environ _environ

char* getenv(const char* name);
int system(const char* command);
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*));
void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));
int abs(int j);
long labs(long j);
long long llabs(long long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

typedef struct localeinfo_struct* _locale_t;

double _strtod_l(const char* nptr, char** endptr, _locale_t locale);
long long _strtoi64_l(const char* nptr, char** endptr, int base,
                      _locale_t locale);
unsigned long long _strtoui64_l(const char* nptr, char** endptr, int base,
                                _locale_t locale);
int mblen(const char* s, size_t n);
int mbtowc(wchar_t* pwc, const char* s, size_t n);
int wctomb(char* s, wchar_t wc);
size_t mbstowcs(wchar_t* dest, const char* src, size_t n);
size_t wcstombs(char* dest, const wchar_t* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

char* realpath(const char* path, char* resolved);
char* _fullpath(char* abspath, const char* relpath, size_t maxlen);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
int mkstemp(char* tmpl);
int mkstemps(char* tmpl, int suffixlen);
char* mktemp(char* tmpl);
const char* getprogname(void);
char* mkdtemp(char* templ);
int posix_memalign(void** memptr, size_t alignment, size_t size);
void* aligned_alloc(size_t alignment, size_t size);
__attribute__((noreturn)) void quick_exit(int status);
int at_quick_exit(void (*func)(void));

#ifndef _WIN32 // FIXME
int putenv(char* string);
#endif

#ifdef __cplusplus
}
#endif
