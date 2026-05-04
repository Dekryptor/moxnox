#include <string_view>
#include <limits.h>
#include <wchar.h>
#include "../../modules/lconv.h"

import ucrtbase;
import print;

using namespace std;

static char dot[] = ".";
static char emptystr[] = "";

static lconv posix_lconv = {
    .decimal_point = dot,
    .thousands_sep = emptystr,
    .grouping = emptystr,
    .mon_decimal_point = emptystr,
    .mon_thousands_sep = emptystr,
    .mon_grouping = emptystr,
    .positive_sign = emptystr,
    .negative_sign = emptystr,
    .currency_symbol = emptystr,
    .frac_digits = CHAR_MAX,
    .p_cs_precedes = CHAR_MAX,
    .n_cs_precedes = CHAR_MAX,
    .p_sep_by_space = CHAR_MAX,
    .n_sep_by_space = CHAR_MAX,
    .p_sign_posn = CHAR_MAX,
    .n_sign_posn = CHAR_MAX,
    .int_curr_symbol = emptystr,
    .int_frac_digits = CHAR_MAX,
    .int_p_cs_precedes = CHAR_MAX,
    .int_n_cs_precedes = CHAR_MAX,
    .int_p_sep_by_space = CHAR_MAX,
    .int_n_sep_by_space = CHAR_MAX,
    .int_p_sign_posn = CHAR_MAX,
    .int_n_sign_posn = CHAR_MAX
};

extern "C"
lconv* _localeconv() {
    return &posix_lconv;
}

extern "C"
int _wctob(wint_t wchar) {
    if (wchar < 0xff)
        return (signed char)wchar;
    else
        return EOF;
}

extern "C"
wint_t _btowc(int c) {
    if (c == EOF)
        return WEOF;

    return c & 0xff;
}

extern "C"
wctype_t __wctype(const char* prop) {
    auto sv = string_view(prop);

    if (sv == "alnum")
        return _DIGIT | _ALPHA;
    else if (sv == "alpha")
        return _ALPHA;
    else if (sv == "cntrl")
        return _CONTROL;
    else if (sv == "digit")
        return _DIGIT;
    else if (sv == "graph")
        return _DIGIT | _PUNCT | _ALPHA;
    else if (sv == "lower")
        return _LOWER;
    else if (sv == "print")
        return _DIGIT | _PUNCT | _BLANK | _ALPHA;
    else if (sv == "punct")
        return _PUNCT;
    else if (sv == "space")
        return _SPACE;
    else if (sv == "upper")
        return _UPPER;
    else if (sv == "xdigit")
        return _HEX;

    return 0;
}

extern "C"
char* setlocale(int category, const char* locale) {
    print("FIXME - setlocale ({}, {})\n", category, locale ? locale : "");

    // FIXME

    return nullptr;
}
