// SPDX-License-Identifier: LGPL-2.1-or-later
#include <array>
#include <wchar.h>

import ucrtbase;

using namespace std;

static const array types = {
    _CONTROL, // 0x00
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL | _BLANK | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL,
    _CONTROL,
    _CONTROL, // 0x10
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _BLANK | _SPACE, // 0x20
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _DIGIT | _HEX, // 0x30
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT, // 0x40
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER, // 0x50
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT, // 0x60
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER, // 0x70
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _CONTROL,
};

extern "C"
int _isblank(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _BLANK;
}

extern "C"
int __toupper(int c) {
    // FIXME - do this properly

    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

extern "C"
int _isalpha(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & (_UPPER | _LOWER);
}

extern "C"
int _isupper(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _UPPER;
}

extern "C"
int _islower(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _LOWER;
}

extern "C"
int _isprint(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return c == ' ' || (types[(size_t)c] & (_UPPER | _LOWER | _DIGIT | _PUNCT));
}

extern "C"
int _iscntrl(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _CONTROL;
}

extern "C"
int _ispunct(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _PUNCT;
}

extern "C"
int _isgraph(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & (_UPPER | _LOWER | _DIGIT | _PUNCT);
}

extern "C"
int _isalnum(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & (_UPPER | _LOWER | _DIGIT);
}

extern "C"
int _iswctype(wint_t c, wctype_t desc) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & desc;
}

extern "C"
int iswalnum(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & (_UPPER | _LOWER | _DIGIT);
}

extern "C"
int iswalpha(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & (_UPPER | _LOWER);
}

extern "C"
int iswblank(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _BLANK;
}

extern "C"
int iswcntrl(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _CONTROL;
}

extern "C"
int iswdigit(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _DIGIT;
}

extern "C"
int iswgraph(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & (_UPPER | _LOWER | _DIGIT | _PUNCT);
}

extern "C"
int iswlower(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _LOWER;
}

extern "C"
int iswprint(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return c == ' ' || (types[(size_t)c] & (_UPPER | _LOWER | _DIGIT | _PUNCT));
}

extern "C"
int iswpunct(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _PUNCT;
}

extern "C"
int iswupper(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _UPPER;
}

extern "C"
int iswxdigit(wint_t c) {
    if ((size_t)c >= types.size())
        return 0;

    return types[(size_t)c] & _HEX;
}
