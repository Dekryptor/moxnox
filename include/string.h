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
#include <strings.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void* memchr(const void* s, int c, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
int strcoll(const char* s1, const char* s2);
size_t strxfrm(char* dest, const char* src, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
size_t strcspn(const char* s, const char* reject);
size_t strspn(const char* s, const char* accept);
char* strpbrk(const char* s, const char* accept);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* str, const char* delim);
size_t strlen(const char* s);
size_t strnlen(const char* s, size_t maxlen);
char* strerror(int errnum);
int strerror_r(int errnum, char* strerrbuf, size_t buflen);

typedef struct localeinfo_struct* _locale_t;

int _strcoll_l(const char* s1, const char* s2, _locale_t locale);
char* strdup(const char* s);
char* _strdup(const char* s); // FIXME
int strncasecmp(const char* s1, const char* s2, size_t n);
int _stricmp(const char* s1, const char* s2); // FIXME
char* strcasestr(const char* haystack, const char* needle);
void* memmem(const void* haystack, size_t haystacklen, const void* needle,
             size_t needlelen);
int _wcsicmp(const wchar_t* s1, const wchar_t* s2);
char* strndup(const char* s, size_t size);
char* strtok_r(char* s, const char* sep, char** state);

#ifdef __cplusplus
}
#endif
