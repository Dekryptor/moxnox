#pragma once

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

#ifdef __cplusplus
#define NULL __null
#else
#define NULL ((void*)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

typedef struct {
    long long __max_align_ll
        __attribute__((__aligned__(__alignof__(long long))));
    long double __max_align_ld
        __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
