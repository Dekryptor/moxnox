include(CheckCSourceCompiles)
include(CMakePushCheckState)

add_library(pixman SHARED
    pixman/pixman/pixman.c
    pixman/pixman/pixman-access.c
    pixman/pixman/pixman-access-accessors.c
    pixman/pixman/pixman-arm.c
    pixman/pixman/pixman-bits-image.c
    pixman/pixman/pixman-combine32.c
    pixman/pixman/pixman-combine-float.c
    pixman/pixman/pixman-conical-gradient.c
    pixman/pixman/pixman-edge.c
    pixman/pixman/pixman-edge-accessors.c
    pixman/pixman/pixman-fast-path.c
    pixman/pixman/pixman-filter.c
    pixman/pixman/pixman-glyph.c
    pixman/pixman/pixman-general.c
    pixman/pixman/pixman-gradient-walker.c
    pixman/pixman/pixman-image.c
    pixman/pixman/pixman-implementation.c
    pixman/pixman/pixman-linear-gradient.c
    pixman/pixman/pixman-matrix.c
    pixman/pixman/pixman-mips.c
    pixman/pixman/pixman-noop.c
    pixman/pixman/pixman-ppc.c
    pixman/pixman/pixman-radial-gradient.c
    pixman/pixman/pixman-region16.c
    pixman/pixman/pixman-region32.c
    pixman/pixman/pixman-region64f.c
    pixman/pixman/pixman-riscv.c
    pixman/pixman/pixman-solid-fill.c
    pixman/pixman/pixman-timer.c
    pixman/pixman/pixman-trap.c
    pixman/pixman/pixman-utils.c
    pixman/pixman/pixman-x86.c
)

set_target_properties(pixman PROPERTIES
    VERSION 0.46.4
)

# FIXME - shouldn't have to specify twice
set(PIXMAN_VERSION_MAJOR 0)
set(PIXMAN_VERSION_MINOR 46)
set(PIXMAN_VERSION_MICRO 4)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/pixman/pixman/pixman-version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pixman/pixman-version.h"
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${USERSPACE_CFLAGS}")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

    set(PIXMAN_USE_SSE2 ON) # supported on every amd64 CPU

    check_c_source_compiles("
        #include <mmintrin.h>
        #include <xmmintrin.h>
        #include <emmintrin.h>
        int param;
        int main () {
            __m128i a = _mm_set1_epi32 (param), b = _mm_set1_epi32 (param + 1), c;
            c = _mm_xor_si128 (a, b);
            return _mm_cvtsi128_si32(c);
        }
    " PIXMAN_USE_SSSE3)

    cmake_pop_check_state()

    set(PIXMAN_USE_X86_MMX ON) # supported on every amd64 CPU

    target_sources(pixman PRIVATE
        pixman/pixman/pixman-mmx.c
        pixman/pixman/pixman-sse2.c
        pixman/pixman/pixman-ssse3.c
    )
else()
    set(PIXMAN_USE_SSE2 OFF)
    set(PIXMAN_USE_SSSE3 OFF)
    set(PIXMAN_USE_X86_MMX OFF)

    # FIXME - NEON support not compiled in
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/pixman-config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pixman/pixman-config.h"
)

add_library(pixman_mod STATIC)
target_sources(pixman_mod PUBLIC FILE_SET CXX_MODULES FILES modules/pixman.cpp)
target_include_directories(pixman_mod PRIVATE ${CMAKE_SOURCE_DIR}/pixman/pixman)
target_include_directories(pixman_mod PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/pixman)
target_compile_options(pixman_mod PRIVATE ${USERSPACE_CFLAGS} -fno-exceptions)

target_include_directories(pixman PRIVATE ${CMAKE_SOURCE_DIR}/pixman/pixman)
target_include_directories(pixman PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/pixman)
target_compile_definitions(pixman PRIVATE -DHAVE_CONFIG_H)
target_compile_definitions(pixman PRIVATE "-DPIXMAN_API=__declspec(dllexport)")
target_compile_options(pixman PRIVATE ${USERSPACE_CFLAGS} -Wno-conversion -Wno-unused-parameter -Wno-type-limits -Wno-sign-compare -Wno-missing-field-initializers -Wno-unused-local-typedefs -Wno-unknown-attributes)
target_compile_options(pixman PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -mno-stack-arg-probe)

target_link_libraries(pixman PRIVATE moxnox)
target_link_libraries(pixman PUBLIC pixman_mod)

install(TARGETS pixman DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
