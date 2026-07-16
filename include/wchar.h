#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifndef _WCTYPE_T_DEFINED
#define _WCTYPE_T_DEFINED
typedef unsigned short wint_t;
typedef unsigned short wctype_t;
#endif

typedef int mbstate_t;

#define WEOF (wint_t)(0xffff)

#define WCHAR_MAX 0xffffU

struct tm;

#ifdef __cplusplus
extern "C" {
#endif

wchar_t* wcscpy(wchar_t* dest, const wchar_t* src);
wchar_t* wcsncpy(wchar_t* dest, const wchar_t* src, size_t n);
wchar_t* wcscat(wchar_t* dest, const wchar_t* src);
wchar_t* wcsncat(wchar_t* dest, const wchar_t* src, size_t n);
int wcscmp(const wchar_t* s1, const wchar_t* s2);
int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n);
int wcscoll(const wchar_t* s1, const wchar_t* s2);
size_t wcsxfrm(wchar_t* dest, const wchar_t* src, size_t n);
wchar_t* wcschr(const wchar_t* s, wchar_t c);
wchar_t* wcsrchr(const wchar_t* s, wchar_t c);
size_t wcscspn(const wchar_t* s, const wchar_t* reject);
size_t wcsspn(const wchar_t* s, const wchar_t* accept);
wchar_t* wcspbrk(const wchar_t* s, const wchar_t* accept);
wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle);
wchar_t* wcstok(wchar_t* str, const wchar_t* delim, wchar_t** saveptr);
size_t wcslen(const wchar_t* s);
wchar_t* wmemchr(const wchar_t* s, wchar_t c, size_t n);
int wmemcmp(const wchar_t* s1, const wchar_t* s2, size_t n);
wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t n);
wchar_t* wmemmove(wchar_t* dest, const wchar_t* src, size_t n);
wchar_t* wmemset(wchar_t* s, wchar_t c, size_t n);
double wcstod(const wchar_t* nptr, wchar_t** endptr);
float wcstof(const wchar_t* nptr, wchar_t** endptr);
long double wcstold(const wchar_t* nptr, wchar_t** endptr);
long wcstol(const wchar_t* nptr, wchar_t** endptr, int base);
long long wcstoll(const wchar_t* nptr, wchar_t** endptr, int base);
unsigned long wcstoul(const wchar_t* nptr, wchar_t** endptr, int base);
unsigned long long wcstoull(const wchar_t* nptr, wchar_t** endptr, int base);
wint_t btowc(int c);
int wctob(wint_t c);
int mbsinit(const mbstate_t* ps);
size_t mbrlen(const char* s, size_t n, mbstate_t* ps);
size_t mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps);
size_t wcrtomb(char* s, wchar_t wc, mbstate_t* ps);
size_t mbsrtowcs(wchar_t* dest, const char** src, size_t len, mbstate_t* ps);
size_t wcsrtombs(char* dest, const wchar_t** src, size_t len, mbstate_t* ps);
size_t wcsftime(wchar_t* s, size_t maxsize, const wchar_t* format,
                const struct tm* timeptr);
wint_t fgetwc(FILE* stream);
wchar_t* fgetws(wchar_t* ws, int n, FILE* stream);
wint_t fputwc(wchar_t wc, FILE* stream);
int fputws(const wchar_t* ws, FILE* stream);
int fwide(FILE* stream, int mode);
wint_t getwc(FILE* stream);
wint_t getwchar(void);
wint_t putwc(wchar_t wc, FILE* stream);
wint_t putwchar(wchar_t wc);
wint_t ungetwc(wint_t wc, FILE* stream);
int fwprintf(FILE* stream, const wchar_t* format, ...);
int fwscanf(FILE* stream, const wchar_t* format, ...);
int swprintf(wchar_t* ws, size_t n, const wchar_t* format, ...);
int swscanf(const wchar_t* ws, const wchar_t* format, ...);
int wprintf(const wchar_t* format, ...);
int wscanf(const wchar_t* format, ...);
int vfwprintf(FILE* stream, const wchar_t* format, va_list arg);
int vfwscanf(FILE* stream, const wchar_t* format, va_list arg);
int vswprintf(wchar_t* ws, size_t n, const wchar_t* format, va_list arg);
int vswscanf(const wchar_t* ws, const wchar_t* format, va_list arg);
int vwprintf(const wchar_t* format, va_list arg);
int vwscanf(const wchar_t* format, va_list arg);
int wcsnicmp(const wchar_t* s1, const wchar_t* s2, size_t n);
int wcwidth(wchar_t wc);

#ifdef __cplusplus
}
#endif
