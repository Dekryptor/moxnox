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
    repo/mingw-w64-crt/stdio/ucrt_ms_fwprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vfprintf.c
    repo/mingw-w64-crt/stdio/ucrt_snprintf.c
    repo/mingw-w64-crt/stdio/ucrt_sprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vsprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vprintf.c
    repo/mingw-w64-crt/stdio/ucrt_sscanf.c
    repo/mingw-w64-crt/stdio/ucrt_fscanf.c
    repo/mingw-w64-crt/stdio/ucrt_scanf.c
    repo/mingw-w64-crt/stdio/ucrt_vfscanf.c
    repo/mingw-w64-crt/stdio/ucrt_vscanf.c
    repo/mingw-w64-crt/stdio/ucrt_vsscanf.c
    repo/mingw-w64-crt/stdio/ucrt_swprintf.c
    repo/mingw-w64-crt/stdio/ucrt__vscprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vsnprintf.c
    repo/mingw-w64-crt/stdio/ucrt__vsnprintf.c
    repo/mingw-w64-crt/stdio/ucrt___local_stdio_printf_options.c
    repo/mingw-w64-crt/stdio/ucrt___local_stdio_scanf_options.c
    repo/mingw-w64-crt/stdio/ucrt_printf.c
    redirects.S
)
target_include_directories(ucrtbase_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include)

if(MINGW_HEADERS)
    target_include_directories(ucrtbase_static PRIVATE ${MINGW_HEADERS})
endif()

target_compile_definitions(ucrtbase_static PRIVATE -D_CRTIMP= -D_WIN32 -D_WIN64)
target_compile_options(ucrtbase_static PRIVATE -ffreestanding)
target_compile_options(ucrtbase_static PRIVATE -Wno-include-next-absolute-path)

add_library(mingwex STATIC
    repo/mingw-w64-crt/crt/loadcfg.S
    repo/mingw-w64-crt/cfguard/mingw_cfguard_support.c
    repo/mingw-w64-crt/misc/dllmain.c
    repo/mingw-w64-crt/complex/_cabs.c
    repo/mingw-w64-crt/complex/cabs.c
    repo/mingw-w64-crt/complex/cabsf.c
    repo/mingw-w64-crt/complex/cabsl.c
    repo/mingw-w64-crt/complex/cacos.c
    repo/mingw-w64-crt/complex/cacosf.c
    repo/mingw-w64-crt/complex/cacosl.c
    repo/mingw-w64-crt/complex/carg.c
    repo/mingw-w64-crt/complex/cargf.c
    repo/mingw-w64-crt/complex/cargl.c
    repo/mingw-w64-crt/complex/casin.c
    repo/mingw-w64-crt/complex/casinf.c
    repo/mingw-w64-crt/complex/casinl.c
    repo/mingw-w64-crt/complex/catan.c
    repo/mingw-w64-crt/complex/catanf.c
    repo/mingw-w64-crt/complex/catanl.c
    repo/mingw-w64-crt/complex/ccos.c
    repo/mingw-w64-crt/complex/ccosf.c
    repo/mingw-w64-crt/complex/ccosl.c
    repo/mingw-w64-crt/complex/cexp.c
    repo/mingw-w64-crt/complex/cexpf.c
    repo/mingw-w64-crt/complex/cexpl.c
    repo/mingw-w64-crt/complex/cimag.c
    repo/mingw-w64-crt/complex/cimagf.c
    repo/mingw-w64-crt/complex/cimagl.c
    repo/mingw-w64-crt/complex/clog.c
    repo/mingw-w64-crt/complex/clogf.c
    repo/mingw-w64-crt/complex/clogl.c
    repo/mingw-w64-crt/complex/clog10.c
    repo/mingw-w64-crt/complex/clog10f.c
    repo/mingw-w64-crt/complex/clog10l.c
    repo/mingw-w64-crt/complex/conj.c
    repo/mingw-w64-crt/complex/conjf.c
    repo/mingw-w64-crt/complex/conjl.c
    repo/mingw-w64-crt/complex/cpow.c
    repo/mingw-w64-crt/complex/cpowf.c
    repo/mingw-w64-crt/complex/cpowl.c
    repo/mingw-w64-crt/complex/cproj.c
    repo/mingw-w64-crt/complex/cprojf.c
    repo/mingw-w64-crt/complex/cprojl.c
    repo/mingw-w64-crt/complex/creal.c
    repo/mingw-w64-crt/complex/crealf.c
    repo/mingw-w64-crt/complex/creall.c
    repo/mingw-w64-crt/complex/csin.c
    repo/mingw-w64-crt/complex/csinf.c
    repo/mingw-w64-crt/complex/csinl.c
    repo/mingw-w64-crt/complex/csqrt.c
    repo/mingw-w64-crt/complex/csqrtf.c
    repo/mingw-w64-crt/complex/csqrtl.c
    repo/mingw-w64-crt/complex/ctan.c
    repo/mingw-w64-crt/complex/ctanf.c
    repo/mingw-w64-crt/complex/ctanl.c
    repo/mingw-w64-crt/gdtoa/arithchk.c
    repo/mingw-w64-crt/gdtoa/dmisc.c
    repo/mingw-w64-crt/gdtoa/dtoa.c
    repo/mingw-w64-crt/gdtoa/g_dfmt.c
    repo/mingw-w64-crt/gdtoa/gdtoa.c
    repo/mingw-w64-crt/gdtoa/gethex.c
    repo/mingw-w64-crt/gdtoa/g_ffmt.c
    repo/mingw-w64-crt/gdtoa/g__fmt.c
    repo/mingw-w64-crt/gdtoa/gmisc.c
    repo/mingw-w64-crt/gdtoa/g_xfmt.c
    repo/mingw-w64-crt/gdtoa/hd_init.c
    repo/mingw-w64-crt/gdtoa/hexnan.c
    repo/mingw-w64-crt/gdtoa/misc.c
    repo/mingw-w64-crt/gdtoa/qnan.c
    repo/mingw-w64-crt/gdtoa/smisc.c
    repo/mingw-w64-crt/gdtoa/strtodg.c
    repo/mingw-w64-crt/gdtoa/strtodnrp.c
    repo/mingw-w64-crt/gdtoa/strtof.c
    repo/mingw-w64-crt/gdtoa/strtopx.c
    repo/mingw-w64-crt/gdtoa/sum.c
    repo/mingw-w64-crt/gdtoa/ulp.c
    repo/mingw-w64-crt/math/acospi.c
    repo/mingw-w64-crt/math/acospif.c
    repo/mingw-w64-crt/math/acospil.c
    repo/mingw-w64-crt/math/asinpi.c
    repo/mingw-w64-crt/math/asinpif.c
    repo/mingw-w64-crt/math/asinpil.c
    repo/mingw-w64-crt/math/atan2pi.c
    repo/mingw-w64-crt/math/atan2pif.c
    repo/mingw-w64-crt/math/atan2pil.c
    repo/mingw-w64-crt/math/atanpi.c
    repo/mingw-w64-crt/math/atanpif.c
    repo/mingw-w64-crt/math/atanpil.c
    repo/mingw-w64-crt/math/coshl.c
    repo/mingw-w64-crt/math/cospi.c
    repo/mingw-w64-crt/math/cospif.c
    repo/mingw-w64-crt/math/cospil.c
    repo/mingw-w64-crt/math/fabsl.c
    repo/mingw-w64-crt/math/fp_consts.c
    repo/mingw-w64-crt/math/fp_constsf.c
    repo/mingw-w64-crt/math/fp_constsl.c
    repo/mingw-w64-crt/math/fpclassify.c
    repo/mingw-w64-crt/math/fpclassifyf.c
    repo/mingw-w64-crt/math/fpclassifyl.c
    repo/mingw-w64-crt/math/frexpf.c
    repo/mingw-w64-crt/math/frexpl.c
    repo/mingw-w64-crt/math/hypotf.c
    repo/mingw-w64-crt/math/hypotl.c
    repo/mingw-w64-crt/math/isnan.c
    repo/mingw-w64-crt/math/isnanf.c
    repo/mingw-w64-crt/math/isnanl.c
    repo/mingw-w64-crt/math/ldexpf.c
    repo/mingw-w64-crt/math/lgamma.c
    repo/mingw-w64-crt/math/lgammaf.c
    repo/mingw-w64-crt/math/lgammal.c
    repo/mingw-w64-crt/math/modfl.c
    repo/mingw-w64-crt/math/powi.c
    repo/mingw-w64-crt/math/powif.c
    repo/mingw-w64-crt/math/powil.c
    repo/mingw-w64-crt/math/signbit.c
    repo/mingw-w64-crt/math/signbitf.c
    repo/mingw-w64-crt/math/signbitl.c
    repo/mingw-w64-crt/math/signgam.c
    repo/mingw-w64-crt/math/sinhl.c
    repo/mingw-w64-crt/math/sinpi.c
    repo/mingw-w64-crt/math/sinpif.c
    repo/mingw-w64-crt/math/sinpil.c
    repo/mingw-w64-crt/math/sqrtl.c
    repo/mingw-w64-crt/math/tanhl.c
    repo/mingw-w64-crt/math/tanpi.c
    repo/mingw-w64-crt/math/tanpif.c
    repo/mingw-w64-crt/math/tanpil.c
    # repo/mingw-w64-crt/misc/_assert.c
    repo/mingw-w64-crt/misc/mingw_longjmp.S
    repo/mingw-w64-crt/misc/mingw_getsp.S
    repo/mingw-w64-crt/misc/alarm.c
    repo/mingw-w64-crt/misc/delay-f.c
    repo/mingw-w64-crt/misc/delay-n.c
    repo/mingw-w64-crt/misc/delayimp.c
    repo/mingw-w64-crt/misc/feclearexcept.c
    repo/mingw-w64-crt/misc/fegetenv.c
    repo/mingw-w64-crt/misc/fegetexceptflag.c
    repo/mingw-w64-crt/misc/fegetround.c
    repo/mingw-w64-crt/misc/feholdexcept.c
    repo/mingw-w64-crt/misc/feraiseexcept.c
    repo/mingw-w64-crt/misc/fesetenv.c
    repo/mingw-w64-crt/misc/fesetexceptflag.c
    repo/mingw-w64-crt/misc/fesetround.c
    repo/mingw-w64-crt/misc/fetestexcept.c
    repo/mingw-w64-crt/misc/feenableexcept.c
    repo/mingw-w64-crt/misc/fedisableexcept.c
    repo/mingw-w64-crt/misc/fegetexcept.c
    repo/mingw-w64-crt/misc/mingw_controlfp.c
    repo/mingw-w64-crt/misc/mingw_setfp.c
    repo/mingw-w64-crt/misc/feupdateenv.c
    repo/mingw-w64-crt/misc/fwide.c
    repo/mingw-w64-crt/misc/getopt.c
    repo/mingw-w64-crt/misc/__mingw_has_sse.c
    repo/mingw-w64-crt/misc/ftime32.c
    # repo/mingw-w64-crt/misc/ftime64.c
    repo/mingw-w64-crt/misc/memalignment.c
    repo/mingw-w64-crt/misc/mingw-aligned-malloc.c
    repo/mingw-w64-crt/misc/mingw_matherr.c
    repo/mingw-w64-crt/misc/mingw_mbwc_convert.c
    repo/mingw-w64-crt/misc/mingw_usleep.c
    repo/mingw-w64-crt/misc/mingw_wcstod.c
    repo/mingw-w64-crt/misc/mingw_wcstof.c
    repo/mingw-w64-crt/misc/mingw_wcstold.c
    repo/mingw-w64-crt/misc/mkstemp.c
    repo/mingw-w64-crt/misc/mkdtemp.c
    repo/mingw-w64-crt/misc/sleep.c
    repo/mingw-w64-crt/misc/strsafe.c
    repo/mingw-w64-crt/misc/tdelete.c
    repo/mingw-w64-crt/misc/tdestroy.c
    repo/mingw-w64-crt/misc/tfind.c
    repo/mingw-w64-crt/misc/tsearch.c
    repo/mingw-w64-crt/misc/twalk.c
    repo/mingw-w64-crt/misc/wcstof.c
    repo/mingw-w64-crt/misc/wcstold.c
    repo/mingw-w64-crt/misc/wdirent.c
    repo/mingw-w64-crt/misc/winbs_uint64.c
    repo/mingw-w64-crt/misc/winbs_ulong.c
    repo/mingw-w64-crt/misc/winbs_ushort.c
    repo/mingw-w64-crt/misc/btowc.c
    repo/mingw-w64-crt/misc/wctob.c
    repo/mingw-w64-crt/misc/wmemchr.c
    repo/mingw-w64-crt/misc/wmemcmp.c
    repo/mingw-w64-crt/misc/wmemcpy.c
    repo/mingw-w64-crt/misc/wmemmove.c
    repo/mingw-w64-crt/misc/wmemset.c
    repo/mingw-w64-crt/misc/mingw-access.c
    repo/mingw-w64-crt/misc/ftw32.c
    repo/mingw-w64-crt/misc/ftw32i64.c
    repo/mingw-w64-crt/misc/ftw64.c
    # repo/mingw-w64-crt/misc/ftw64i32.c
    repo/mingw-w64-crt/misc/__mingw_filename_cp.c
    repo/mingw-w64-crt/ssp/chk_fail.c
    # repo/mingw-w64-crt/ssp/gets_chk.c
    repo/mingw-w64-crt/ssp/memcpy_chk.c
    repo/mingw-w64-crt/ssp/memmove_chk.c
    repo/mingw-w64-crt/ssp/mempcpy_chk.c
    repo/mingw-w64-crt/ssp/memset_chk.c
    stack_chk_fail.cpp
    stack_chk_guard.cpp
    repo/mingw-w64-crt/ssp/strcat_chk.c
    repo/mingw-w64-crt/ssp/stpcpy_chk.c
    repo/mingw-w64-crt/ssp/strcpy_chk.c
    repo/mingw-w64-crt/ssp/strncat_chk.c
    repo/mingw-w64-crt/ssp/strncpy_chk.c
    repo/mingw-w64-crt/stdio/strtok_r.c
    repo/mingw-w64-crt/stdio/_Exit.c
    repo/mingw-w64-crt/stdio/asprintf.c
    repo/mingw-w64-crt/stdio/lltoa.c
    repo/mingw-w64-crt/stdio/lltow.c
    repo/mingw-w64-crt/stdio/__mingw_fix_stat_finish.c
    repo/mingw-w64-crt/stdio/__mingw_fix_fstat_finish.c
    repo/mingw-w64-crt/stdio/__mingw_fix_stat_path.c
    repo/mingw-w64-crt/stdio/__mingw_fix_wstat_path.c
    repo/mingw-w64-crt/stdio/mingw_fprintf.c
    repo/mingw-w64-crt/stdio/mingw_fwprintf.c
    repo/mingw-w64-crt/stdio/mingw_fscanf.c
    repo/mingw-w64-crt/stdio/mingw_fwscanf.c
    repo/mingw-w64-crt/stdio/mingw_pformat.c
    repo/mingw-w64-crt/stdio/mingw_wpformat.c
    repo/mingw-w64-crt/stdio/mingw_printf.c
    repo/mingw-w64-crt/stdio/mingw_wprintf.c
    repo/mingw-w64-crt/stdio/mingw_scanf.c
    repo/mingw-w64-crt/stdio/mingw_snprintf.c
    repo/mingw-w64-crt/stdio/mingw_swprintf.c
    repo/mingw-w64-crt/stdio/mingw_snwprintf.c
    repo/mingw-w64-crt/stdio/mingw_sprintf.c
    repo/mingw-w64-crt/stdio/mingw_sscanf.c
    repo/mingw-w64-crt/stdio/mingw_swscanf.c
    repo/mingw-w64-crt/stdio/mingw_vfprintf.c
    repo/mingw-w64-crt/stdio/mingw_vfwprintf.c
    repo/mingw-w64-crt/stdio/mingw_vfscanf.c
    repo/mingw-w64-crt/stdio/mingw_vprintf.c
    repo/mingw-w64-crt/stdio/mingw_vwprintf.c
    repo/mingw-w64-crt/stdio/mingw_vsnprintf.c
    repo/mingw-w64-crt/stdio/mingw_vswprintf.c
    repo/mingw-w64-crt/stdio/mingw_vsnwprintf.c
    repo/mingw-w64-crt/stdio/mingw_vsprintf.c
    repo/mingw-w64-crt/stdio/mingw_wscanf.c
    repo/mingw-w64-crt/stdio/mingw_vfwscanf.c
    repo/mingw-w64-crt/stdio/mingw_vswscanf.c
    repo/mingw-w64-crt/stdio/mingw_vsscanf.c
    repo/mingw-w64-crt/stdio/mingw_sformat.c
    repo/mingw-w64-crt/stdio/mingw_swformat.c
    repo/mingw-w64-crt/stdio/snprintf.c
    repo/mingw-w64-crt/stdio/snwprintf.c
    repo/mingw-w64-crt/stdio/truncate64.c
    repo/mingw-w64-crt/stdio/ulltoa.c
    repo/mingw-w64-crt/stdio/ulltow.c
    repo/mingw-w64-crt/stdio/vasprintf.c
    repo/mingw-w64-crt/stdio/vsnprintf.c
    repo/mingw-w64-crt/stdio/vsnwprintf.c
    repo/mingw-w64-crt/stdio/wtoll.c
    repo/mingw-w64-crt/stdio/mingw_asprintf.c
    repo/mingw-w64-crt/stdio/mingw_vasprintf.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    target_sources(mingwex PRIVATE
        repo/mingw-w64-crt/math/cbrtl.c
        repo/mingw-w64-crt/math/erfl.c
        repo/mingw-w64-crt/math/fdiml.c
        repo/mingw-w64-crt/math/fmal.c
        repo/mingw-w64-crt/math/fmaxl.c
        repo/mingw-w64-crt/math/fminl.c
        repo/mingw-w64-crt/math/llrintl.c
        repo/mingw-w64-crt/math/llroundl.c
        repo/mingw-w64-crt/math/lrintl.c
        repo/mingw-w64-crt/math/lroundl.c
        repo/mingw-w64-crt/math/rintl.c
        repo/mingw-w64-crt/math/roundl.c
        repo/mingw-w64-crt/math/tgammal.c
        repo/mingw-w64-crt/math/truncl.c
        repo/mingw-w64-crt/math/x86/_chgsignl.S
        repo/mingw-w64-crt/math/x86/acosh.def.h
        repo/mingw-w64-crt/math/x86/acoshl.c
        repo/mingw-w64-crt/math/x86/acosl.c
        repo/mingw-w64-crt/math/x86/asinhl.c
        repo/mingw-w64-crt/math/x86/asinl.c
        repo/mingw-w64-crt/math/x86/atan2l.c
        repo/mingw-w64-crt/math/x86/atanhl.c
        repo/mingw-w64-crt/math/x86/atanl.c
        repo/mingw-w64-crt/math/x86/ceill.S
        repo/mingw-w64-crt/math/x86/copysignl.S
        repo/mingw-w64-crt/math/x86/cos.def.h
        repo/mingw-w64-crt/math/x86/cosl.c
        repo/mingw-w64-crt/math/x86/cosl_internal.S
        repo/mingw-w64-crt/math/x86/cossinl.c
        repo/mingw-w64-crt/math/x86/exp2l.S
        repo/mingw-w64-crt/math/x86/exp.def.h
        repo/mingw-w64-crt/math/x86/expl.c
        repo/mingw-w64-crt/math/x86/expm1.def.h
        repo/mingw-w64-crt/math/x86/expm1l.c
        repo/mingw-w64-crt/math/x86/fastmath.h
        repo/mingw-w64-crt/math/x86/floorl.S
        repo/mingw-w64-crt/math/x86/fmodl.c
        repo/mingw-w64-crt/math/x86/fucom.c
        repo/mingw-w64-crt/math/x86/ilogbl.S
        repo/mingw-w64-crt/math/x86/internal_logl.S
        repo/mingw-w64-crt/math/x86/ldexp.c
        repo/mingw-w64-crt/math/x86/ldexpl.c
        repo/mingw-w64-crt/math/x86/log.def.h
        repo/mingw-w64-crt/math/x86/log10l.S
        repo/mingw-w64-crt/math/x86/log1pl.S
        repo/mingw-w64-crt/math/x86/log2l.S
        repo/mingw-w64-crt/math/x86/logbl.c
        repo/mingw-w64-crt/math/x86/logl.c
        repo/mingw-w64-crt/math/x86/nearbyintl.S
        repo/mingw-w64-crt/math/x86/pow.def.h
        repo/mingw-w64-crt/math/x86/powl.c
        repo/mingw-w64-crt/math/x86/remainderl.S
        repo/mingw-w64-crt/math/x86/remquol.S
        repo/mingw-w64-crt/math/x86/scalbn.S
        repo/mingw-w64-crt/math/x86/scalbnf.S
        repo/mingw-w64-crt/math/x86/scalbnl.S
        repo/mingw-w64-crt/math/x86/sin.def.h
        repo/mingw-w64-crt/math/x86/sinl.c
        repo/mingw-w64-crt/math/x86/sinl_internal.S
        repo/mingw-w64-crt/math/x86/tanl.S
        repo/mingw-w64-crt/cfguard/guard_dispatch.S
    )
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_sources(mingwex PRIVATE
        repo/mingw-w64-crt/math/sincos.c
        repo/mingw-w64-crt/math/sincosf.c
        repo/mingw-w64-crt/math/arm-common/sincosl.c
        repo/mingw-w64-crt/math/arm64/rint.c
        repo/mingw-w64-crt/math/arm64/rintf.c
        repo/mingw-w64-crt/math/arm-common/ldexpl.c
    )
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-resource-dir
        OUTPUT_VARIABLE CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/float.h CONTENT
"#pragma once
#include \"${CLANG_RESOURCE_DIR}/include/float.h\"
#include \"${CMAKE_CURRENT_SOURCE_DIR}/repo/mingw-w64-headers/crt/float.h\"
")
endif()

target_include_directories(mingwex PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include)

if(MINGW_HEADERS)
    target_include_directories(mingwex PRIVATE ${MINGW_HEADERS})
endif()

target_compile_definitions(mingwex PRIVATE -D_CRTIMP= -D_UCRT -D_CRTBLD -D_WIN32 -D_WIN64 -D__MINGW32__)
target_compile_options(mingwex PRIVATE ${USERSPACE_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions> -mno-stack-arg-probe $<$<CXX_COMPILER_ID:Clang>:-Wno-pragma-pack -Wno-include-next-absolute-path>)
target_compile_options(mingwex PRIVATE -Wno-shorten-64-to-32 -Wno-missing-declarations -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-unused-variable -Wno-gnu-pointer-arith -Wno-implicit-float-conversion -Wno-float-conversion -Wno-conversion -Wno-pointer-arith $<$<CXX_COMPILER_ID:GNU>:-Wno-use-after-free -Wno-missing-attributes>)
target_link_libraries(mingwex PRIVATE ntdll_mod)
