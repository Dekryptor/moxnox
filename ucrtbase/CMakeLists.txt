cmake_minimum_required(VERSION 3.28)

project(ucrtbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(musl STATIC
    ../ports/musl/repo/src/math/floorf.c
    ../ports/musl/repo/src/math/ceilf.c
    ../ports/musl/repo/src/math/sin.c
    ../ports/musl/repo/src/math/cos.c
    ../ports/musl/repo/src/math/tan.c
    ../ports/musl/repo/src/math/__sin.c
    ../ports/musl/repo/src/math/__cos.c
    ../ports/musl/repo/src/math/__tan.c
    ../ports/musl/repo/src/math/__rem_pio2.c
    ../ports/musl/repo/src/math/__rem_pio2_large.c
    ../ports/musl/repo/src/math/ceil.c
    ../ports/musl/repo/src/math/floor.c
    ../ports/musl/repo/src/math/sinf.c
    ../ports/musl/repo/src/math/cosf.c
    ../ports/musl/repo/src/math/tanf.c
    ../ports/musl/repo/src/math/__sindf.c
    ../ports/musl/repo/src/math/__cosdf.c
    ../ports/musl/repo/src/math/__tandf.c
    ../ports/musl/repo/src/math/__rem_pio2f.c
    ../ports/musl/repo/src/math/scalbn.c
    ../ports/musl/repo/src/math/atan2.c
    ../ports/musl/repo/src/math/atan.c
    ../ports/musl/repo/src/math/atan2f.c
    ../ports/musl/repo/src/math/atanf.c
)

target_include_directories(musl PRIVATE musl_compat ../ports/musl/repo/src/internal ../ports/musl/repo/arch/generic)
target_compile_definitions(musl PRIVATE hidden=)

set(SRC_FILES src/ucrtbase.cpp
    src/stubs.S
    src/ucrtbase.def
    src/errno.cpp
    src/data.cpp
    src/heap.cpp
    src/exit.cpp
    src/locale.cpp
    src/mbcs.cpp
    src/math.cpp
    src/misc.cpp
    src/onexit.cpp
    src/environ.cpp
    src/string.cpp
    src/except.cpp
)

add_library(ucrtbase SHARED ${SRC_FILES})
target_sources(ucrtbase PUBLIC FILE_SET CXX_MODULES FILES src/file.cpp)

target_compile_options(ucrtbase PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ucrtbase PUBLIC -nostartfiles -nostdlib)

target_link_libraries(ucrtbase PRIVATE format print unicode list errno)
target_link_libraries(ucrtbase PRIVATE ntdll_mod ucrtbase_mod kernelbase_mod)
target_link_libraries(ucrtbase PRIVATE -Wl,--whole-archive musl -Wl,--no-whole-archive)

install(TARGETS ucrtbase
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
