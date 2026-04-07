set(DEFAULT_MSVCRT_VERSION 0x600)
set(DEFAULT_WIN32_WINNT 0xa00)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/repo/mingw-w64-headers/crt/_mingw.h.in ${CMAKE_CURRENT_BINARY_DIR}/_mingw.h)
unset(DEFAULT_WIN32_WINNT)
unset(DEFAULT_MSVCRT_VERSION)

add_library(ucrtbase_static STATIC
    repo/mingw-w64-crt/misc/ucrt_amsg_exit.c
    repo/mingw-w64-crt/misc/ucrt__getmainargs.c
    repo/mingw-w64-crt/misc/__set_app_type.c
    repo/mingw-w64-crt/misc/__p___initenv.c
    repo/mingw-w64-crt/misc/__initenv.c
    repo/mingw-w64-crt/misc/_onexit.c
    repo/mingw-w64-crt/stdio/ucrt_fprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vfprintf.c
    repo/mingw-w64-crt/stdio/ucrt_snprintf.c
    repo/mingw-w64-crt/stdio/ucrt_sscanf.c
    repo/mingw-w64-crt/stdio/ucrt_swprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vsnprintf.c
    repo/mingw-w64-crt/stdio/ucrt___local_stdio_printf_options.c
    repo/mingw-w64-crt/stdio/ucrt___local_stdio_scanf_options.c
)
target_include_directories(ucrtbase_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include)
target_compile_definitions(ucrtbase_static PRIVATE -D_CRTIMP=)
target_compile_options(ucrtbase_static PRIVATE -ffreestanding)
