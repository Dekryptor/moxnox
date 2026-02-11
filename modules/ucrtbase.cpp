module;

#include <stdint.h>

export module ucrtbase;

export {

using _invalid_parameter_handler = void (*)(const wchar_t*, const wchar_t*, const wchar_t*,
                                            unsigned int, uintptr_t);

extern "C" {
_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler handler);

}

}
