// SPDX-License-Identifier: MIT
#pragma once

#include <wchar.h>
#include <stddef.h>
#include <stdint.h>

typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;

#ifdef __cplusplus
extern "C" {
#endif

size_t c16rtomb(char* s, char16_t c16, mbstate_t* ps);
size_t c32rtomb(char* s, char32_t c32, mbstate_t* ps);
size_t mbrtoc16(char16_t* pc16, const char* s, size_t n, mbstate_t* ps);
size_t mbrtoc32(char32_t* pc32, const char* s, size_t n, mbstate_t* ps);

#ifdef __cplusplus
}
#endif
