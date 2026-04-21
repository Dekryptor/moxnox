cmake_minimum_required(VERSION 3.28)

project(ucrtbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(musl STATIC
    ../ports/musl/repo/src/math/floorf.c
    ../ports/musl/repo/src/math/ceilf.c
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
