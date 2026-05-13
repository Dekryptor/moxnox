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
    ../ports/musl/repo/src/math/acos.c
    ../ports/musl/repo/src/math/acosf.c
    ../ports/musl/repo/src/math/sqrt.c
    ../ports/musl/repo/src/math/sqrtf.c
    ../ports/musl/repo/src/math/sqrt_data.c
    ../ports/musl/repo/src/math/__math_invalid.c
    ../ports/musl/repo/src/math/__math_invalidf.c
    ../ports/musl/repo/src/math/hypot.c
    ../ports/musl/repo/src/math/hypotf.c
    ../ports/musl/repo/src/math/sincos.c
    ../ports/musl/repo/src/math/sincosf.c
    ../ports/musl/repo/src/math/trunc.c
    ../ports/musl/repo/src/math/truncf.c
    ../ports/musl/repo/src/math/exp.c
    ../ports/musl/repo/src/math/expf.c
    ../ports/musl/repo/src/math/exp_data.c
    ../ports/musl/repo/src/math/exp2f_data.c
    ../ports/musl/repo/src/math/__math_oflow.c
    ../ports/musl/repo/src/math/__math_oflowf.c
    ../ports/musl/repo/src/math/__math_uflow.c
    ../ports/musl/repo/src/math/__math_uflowf.c
    ../ports/musl/repo/src/math/__math_xflow.c
    ../ports/musl/repo/src/math/__math_xflowf.c
    ../ports/musl/repo/src/math/pow.c
    ../ports/musl/repo/src/math/powf.c
    ../ports/musl/repo/src/math/pow_data.c
    ../ports/musl/repo/src/math/powf_data.c
    ../ports/musl/repo/src/math/log.c
    ../ports/musl/repo/src/math/logf.c
    ../ports/musl/repo/src/math/log_data.c
    ../ports/musl/repo/src/math/logf_data.c
    ../ports/musl/repo/src/math/__math_divzero.c
    ../ports/musl/repo/src/math/__math_divzerof.c
    ../ports/musl/repo/src/math/log10.c
    ../ports/musl/repo/src/math/log10f.c
    ../ports/musl/repo/src/math/sinh.c
    ../ports/musl/repo/src/math/sinhf.c
    ../ports/musl/repo/src/math/expm1.c
    ../ports/musl/repo/src/math/expm1f.c
    ../ports/musl/repo/src/math/__expo2.c
    ../ports/musl/repo/src/math/__expo2f.c
    ../ports/musl/repo/src/math/cosh.c
    ../ports/musl/repo/src/math/coshf.c
    ../ports/musl/repo/src/math/tanh.c
    ../ports/musl/repo/src/math/tanhf.c
)

target_include_directories(musl PRIVATE musl_compat ../ports/musl/repo/src/include ../ports/musl/repo/src/internal ../ports/musl/repo/arch/generic)
target_compile_options(musl PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/../ports/musl/repo/src/include/features.h)
target_compile_options(musl PRIVATE -Wno-attributes)

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
    src/setjmp.cpp
    src/dir.cpp
    src/ctype.cpp
)

add_library(ucrtbase SHARED ${SRC_FILES})
target_sources(ucrtbase PUBLIC FILE_SET CXX_MODULES FILES
    src/file.cpp
    src/time.cpp
)

target_compile_options(ucrtbase PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ucrtbase PRIVATE -nostartfiles -nostdlib)

target_link_libraries(ucrtbase PRIVATE format print unicode list errno unique_handle moxnox_ioctls)
target_link_libraries(ucrtbase PRIVATE ntdll_mod ucrtbase_mod kernelbase_mod)
target_link_libraries(ucrtbase PRIVATE -Wl,--whole-archive musl -Wl,--no-whole-archive)

install(TARGETS ucrtbase
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
