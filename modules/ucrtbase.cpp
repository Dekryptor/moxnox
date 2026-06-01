module;

#define _CRT_ERRNO_DEFINED
#include <stdint.h>
#include <wchar.h>
#include <wctype.h>
#include "lconv.h"

export module ucrtbase;

export using ::mbstate_t;

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

using _PVFV = void (*)();
using _PIFV = int (*)();

const int EOF = -1;

using _onexit_t = int (*)(void);
using _tls_callback_type = void (*)(void* dll_base, uint32_t reason,
                                    void* reserved);

const int _O_RDONLY = 0x0000;
const int _O_WRONLY = 0x0001;
const int _O_RDWR = 0x0002;
const int _O_APPEND = 0x0008;
const int _O_RANDOM = 0x0010;
const int _O_SEQUENTIAL = 0x0020;
const int _O_TEMPORARY = 0x0040;
const int _O_NOINHERIT = 0x0080;
const int _O_CREAT = 0x0100;
const int _O_TRUNC = 0x0200;
const int _O_EXCL = 0x0400;
const int _O_SHORT_LIVED = 0x1000;
const int _O_TEXT = 0x4000;
const int _O_BINARY = 0x8000;
const int _O_WTEXT = 0x10000;
const int _O_U16TEXT = 0x20000;
const int _O_U8TEXT = 0x40000;

const int _S_IEXEC = 0100;
const int _S_IWRITE = 0200;
const int _S_IREAD = 0400;
const int _S_IFIFO = 010000;
const int _S_IFCHR = 020000;
const int _S_IFDIR = 040000;
const int _S_IFREG = 0100000;
const int _S_IFMT = 0170000;

extern "C" {

struct _onexit_table_t {
    _PVFV* _first;
    _PVFV* _last;
    _PVFV* _end;
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
FILE*  __attribute__((__dllimport__)) __acrt_iob_func(unsigned int idx);
void _lock_file(FILE* file);
void _unlock_file(FILE* file);
int* _errno();
int _fputc_nolock(int c, FILE* file);
struct lconv* localeconv();
void _fpreset();
int _initialize_onexit_table(_onexit_table_t* table);
int _register_onexit_function(_onexit_table_t* table, _onexit_t func);
int _execute_onexit_table(_onexit_table_t* table);
char* getenv(const char* name);
errno_t getenv_s(size_t* len, char* value, rsize_t valuesz, const char* name);
char* _strdup(const char* s);
void _register_thread_local_exe_atexit_callback(_tls_callback_type callback);
void* _aligned_malloc(size_t size, size_t alignment);
void _aligned_free(void* ptr);
int setvbuf(FILE* stream, char* buffer, int mode, size_t size);
int _open(const char* path, int flags, ...);
intptr_t _get_osfhandle(int fd);
char* _getcwd(char *buffer, int maxlen);
int _open_osfhandle(intptr_t osfhandle, int flags);
int _close(int fd);

}

}
