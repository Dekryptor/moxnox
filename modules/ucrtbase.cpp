module;

#include <stdint.h>

export module ucrtbase;

export {

using _invalid_parameter_handler = void (*)(const wchar_t*, const wchar_t*, const wchar_t*,
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

extern "C" {

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

}

}
