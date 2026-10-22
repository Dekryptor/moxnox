// SPDX-License-Identifier: MIT
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int tolower(int c);
int toupper(int c);

typedef struct localeinfo_struct* _locale_t;

int _isdigit_l(int c, _locale_t locale);
int _isxdigit_l(int c, _locale_t locale);

#ifdef __cplusplus
}
#endif

#define isascii(c) ((unsigned int)(c) < 0x80)
