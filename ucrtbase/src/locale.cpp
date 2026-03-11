#include <limits.h>
#include <wchar.h>

import ucrtbase;
import print;

static char dot[] = ".";
static char empty[] = "";

static lconv posix_lconv = {
    .decimal_point = dot,
    .thousands_sep = empty,
    .grouping = empty,
    .mon_decimal_point = empty,
    .mon_thousands_sep = empty,
    .mon_grouping = empty,
    .positive_sign = empty,
    .negative_sign = empty,
    .currency_symbol = empty,
    .frac_digits = CHAR_MAX,
    .p_cs_precedes = CHAR_MAX,
    .n_cs_precedes = CHAR_MAX,
    .p_sep_by_space = CHAR_MAX,
    .n_sep_by_space = CHAR_MAX,
    .p_sign_posn = CHAR_MAX,
    .n_sign_posn = CHAR_MAX,
    .int_curr_symbol = empty,
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
int wctob(wint_t wchar) {
    print_stderr("FIXME - wctob {}\n", wchar); // FIXME

    return -1;
}

extern "C"
wint_t btowc(int c) {
    print_stderr("FIXME - btowc {}\n", c); // FIXME

    return WEOF;
}

extern "C"
wctype_t wctype(const char* prop) {
    print_stderr("FIXME - wctype \"{}\"\n", prop); // FIXME

    return 0;
}
