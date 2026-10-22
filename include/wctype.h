// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>

#ifndef _WCTYPE_T_DEFINED
#define _WCTYPE_T_DEFINED
typedef unsigned short wint_t;
typedef unsigned short wctype_t;
#endif

typedef wchar_t wctrans_t;

#define _UPPER 0x1
#define _LOWER 0x2
#define _DIGIT 0x4
#define _SPACE 0x8
#define _PUNCT 0x10
#define _CONTROL 0x20
#define _BLANK 0x40
#define _HEX 0x80
#define _ALPHA (0x0100 | _UPPER | _LOWER)

// FIXME
#undef iswalnum
#undef iswalpha
#undef iswblank
#undef iswcntrl
#undef iswdigit
#undef iswgraph
#undef iswlower
#undef iswprint
#undef iswpunct
#undef iswspace
#undef iswupper
#undef iswxdigit

#ifdef __cplusplus
extern "C" {
#endif

int iswalnum(wint_t wc);
int iswalpha(wint_t wc);
int iswblank(wint_t wc);
int iswcntrl(wint_t wc);
int iswdigit(wint_t wc);
int iswgraph(wint_t wc);
int iswlower(wint_t wc);
int iswprint(wint_t wc);
int iswpunct(wint_t wc);
int iswspace(wint_t wc);
int iswupper(wint_t wc);
int iswxdigit(wint_t wc);
int iswctype(wint_t wc, wctype_t desc);
wctype_t wctype(const char* property);
wint_t towlower(wint_t wc);
wint_t towupper(wint_t wc);
wint_t towctrans(wint_t wc, wctrans_t desc);
wctrans_t wctrans(const char* property);

#ifdef __cplusplus
}
#endif
