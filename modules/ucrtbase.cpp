module;

#include <stdint.h>

export module ucrtbase;

export {

extern "C++" using _invalid_parameter_handler = void (*)(const wchar_t*,
                                                         const wchar_t*,
                                                         const wchar_t*,
                                                         unsigned int, uintptr_t);

enum _crt_app_type {
    _crt_unknown_app,
    _crt_console_app,
    _crt_gui_app
};

const int _O_TEXT = 0x4000;
const int _O_BINARY = 0x8000;

using _PVFV = void (*)();
using _PIFV = int (*)();

const int EOF = -1;

const int EPERM = 1;
const int ENOENT = 2;
const int ESRCH = 3;
const int EINTR = 4;
const int EIO = 5;
const int ENXIO = 6;
const int E2BIG = 7;
const int ENOEXEC = 8;
const int EBADF = 9;
const int ECHILD = 10;
const int EAGAIN = 11;
const int ENOMEM = 12;
const int EACCES = 13;
const int EFAULT = 14;
const int EBUSY = 16;
const int EEXIST = 17;
const int EXDEV = 18;
const int ENODEV = 19;
const int ENOTDIR = 20;
const int EISDIR = 21;
const int EINVAL = 22;
const int ENFILE = 23;
const int EMFILE = 24;
const int ENOTTY = 25;
const int EFBIG = 27;
const int ENOSPC = 28;
const int ESPIPE = 29;
const int EROFS = 30;
const int EMLINK = 31;
const int EPIPE = 32;
const int EDOM = 33;
const int ERANGE = 34;
const int EDEADLK = 36;
const int ENAMETOOLONG = 38;
const int ENOLCK = 39;
const int ENOSYS = 40;
const int ENOTEMPTY = 41;
const int EILSEQ = 42;

extern "C" {

struct FILE;

struct lconv {
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    char* currency_symbol;
    char frac_digits;
    char p_cs_precedes;
    char n_cs_precedes;
    char p_sep_by_space;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char* int_curr_symbol;
    char int_frac_digits;
    char int_p_cs_precedes;
    char int_n_cs_precedes;
    char int_p_sep_by_space;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler handler);
void _set_app_type(_crt_app_type _Type);
int* __p___argc();
char*** __p___argv();
unsigned int* __p__commode();
char*** __p__environ();
int* __p__fmode();
void _initterm(_PVFV* first, _PVFV* last);
int _initterm_e(_PIFV* first, _PIFV* last);
int _initialize_narrow_environment();
int _configure_narrow_argv(int mode);
int _set_new_mode(int new_mode);
int _crt_atexit(_PVFV function);
void exit(int code) noexcept;
int puts(const char* s);
FILE* __acrt_iob_func(unsigned int idx);
void _lock_file(FILE* file);
void _unlock_file(FILE* file);
int* _errno();
int _fputc_nolock(int c, FILE* file);
struct lconv* localeconv();

}

}
