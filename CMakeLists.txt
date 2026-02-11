cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

enable_language(ASM)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(GENERAL_CFLAGS -gdwarf-5 $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++> -march=armv8.1-a)
    set(USERSPACE_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type -fno-rtti -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden ${GENERAL_CFLAGS})
    set(KERNEL_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type -fno-exceptions -fno-rtti -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden ${GENERAL_CFLAGS})
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    set(GENERAL_CFLAGS -gdwarf-5)
    set(USERSPACE_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type -fno-rtti -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden ${GENERAL_CFLAGS})
    set(KERNEL_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type -fno-exceptions -fno-rtti -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -masm=intel ${GENERAL_CFLAGS})
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    add_library(format_kernel STATIC)
    target_sources(format_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
    target_compile_options(format_kernel PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(format_kernel nt_kernel)

    add_library(dwarf_kernel STATIC)
    target_sources(dwarf_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/dwarf.cpp)
    target_compile_options(dwarf_kernel PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(dwarf_kernel format_kernel list_kernel)

    add_library(unwind_kernel STATIC)
    target_sources(unwind_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/unwind.cpp)
    target_compile_options(unwind_kernel PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(unwind_kernel nt_kernel pe_kernel)

    add_library(list_kernel STATIC)
    target_sources(list_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/list.cpp)
    target_compile_options(list_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(unicode_kernel STATIC)
    target_sources(unicode_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/unicode.cpp)
    target_compile_options(unicode_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(rbtree_kernel STATIC)
    target_sources(rbtree_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/rbtree.cpp)
    target_compile_options(rbtree_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(vt100_kernel STATIC)
    target_sources(vt100_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/vt100.cpp)
    target_compile_options(vt100_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(cp437_kernel STATIC)
    target_sources(cp437_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/cp437.cpp)
    target_compile_options(cp437_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(evdev_kernel STATIC)
    target_sources(evdev_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/evdev.cpp)
    target_compile_options(evdev_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(nt_kernel STATIC)
    target_sources(nt_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
    target_compile_options(nt_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(ntdll_kernel STATIC)
    target_sources(ntdll_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/ntdll.cpp)
    target_compile_options(ntdll_kernel PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(ntdll_kernel nt_kernel)

    add_library(pe_kernel STATIC)
    target_sources(pe_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/pe.cpp)
    target_compile_options(pe_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(pe_32bit STATIC)
    target_sources(pe_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/pe.cpp)
    target_compile_options(pe_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(nt_32bit STATIC)
    target_sources(nt_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
    target_compile_options(nt_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(format_32bit STATIC)
    target_sources(format_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
    target_compile_options(format_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)
    target_link_libraries(format_32bit nt_32bit)

    add_library(loaderblock_32bit STATIC)
    target_sources(loaderblock_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/loaderblock.cpp)
    target_compile_options(loaderblock_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)
    target_link_libraries(loaderblock_32bit format_32bit)

    add_library(crt_kernel STATIC modules/crt.S)
    target_sources(crt_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/crt.cpp)
    target_compile_options(crt_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(crt_32bit STATIC)
    target_sources(crt_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/crt.cpp)
    target_compile_options(crt_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(x86 STATIC)
    target_sources(x86 PUBLIC FILE_SET CXX_MODULES FILES modules/x86.cpp)
    target_compile_options(x86 PRIVATE ${KERNEL_CFLAGS})

    add_library(x86_32bit STATIC)
    target_sources(x86_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/x86.cpp)
    target_compile_options(x86_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(pagesize_kernel STATIC)
    target_sources(pagesize_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/pagesize.cpp)
    target_compile_options(pagesize_kernel PRIVATE ${KERNEL_CFLAGS})

    add_library(pagesize_32bit STATIC)
    target_sources(pagesize_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/pagesize.cpp)
    target_compile_options(pagesize_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(timepage_kernel STATIC)
    target_sources(timepage_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/timepage.cpp)
    target_compile_options(timepage_kernel PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(timepage_kernel pagesize_kernel)

    add_library(moxnox_ioctls_kernel STATIC)
    target_sources(moxnox_ioctls_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/moxnox_ioctls.cpp)
    target_compile_options(moxnox_ioctls_kernel PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(moxnox_ioctls_kernel nt_kernel)

    add_library(gnu-efi STATIC)
    target_sources(gnu-efi PUBLIC FILE_SET CXX_MODULES FILES modules/gnu-efi.cpp)
    target_include_directories(gnu-efi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi/inc ${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi/inc/x86_64)
    target_compile_options(gnu-efi PRIVATE ${KERNEL_CFLAGS})
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_library(gnu-efi STATIC)
    target_sources(gnu-efi PUBLIC FILE_SET CXX_MODULES FILES modules/gnu-efi.cpp)
    target_include_directories(gnu-efi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi/inc ${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi/inc/aarch64)
    target_compile_options(gnu-efi PRIVATE ${KERNEL_CFLAGS})
endif()

add_library(format STATIC)
target_sources(format PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(format PRIVATE nt)

add_library(dwarf STATIC)
target_sources(dwarf PUBLIC FILE_SET CXX_MODULES FILES modules/dwarf.cpp)
target_compile_options(dwarf PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(dwarf PRIVATE format list)

add_library(unwind STATIC)
target_sources(unwind PUBLIC FILE_SET CXX_MODULES FILES modules/unwind.cpp)
target_compile_options(unwind PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(unwind PRIVATE nt pe)

add_library(signal STATIC)
target_sources(signal PUBLIC FILE_SET CXX_MODULES FILES modules/signal.cpp)
target_compile_options(signal PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(signal PRIVATE unique_handle pe)
target_link_libraries(signal PRIVATE moxnox)

add_library(list STATIC)
target_sources(list PUBLIC FILE_SET CXX_MODULES FILES modules/list.cpp)
target_compile_options(list PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(unicode STATIC)
target_sources(unicode PUBLIC FILE_SET CXX_MODULES FILES modules/unicode.cpp)
target_compile_options(unicode PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(rbtree STATIC)
target_sources(rbtree PUBLIC FILE_SET CXX_MODULES FILES modules/rbtree.cpp)
target_compile_options(rbtree PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(unique_handle STATIC)
target_sources(unique_handle PUBLIC FILE_SET CXX_MODULES FILES modules/unique_handle.cpp)
target_compile_options(unique_handle PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(unique_handle PRIVATE nt)

add_library(vt100 STATIC)
target_sources(vt100 PUBLIC FILE_SET CXX_MODULES FILES modules/vt100.cpp)
target_compile_options(vt100 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(cp437 STATIC)
target_sources(cp437 PUBLIC FILE_SET CXX_MODULES FILES modules/cp437.cpp)
target_compile_options(cp437 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(cmdline STATIC)
target_sources(cmdline PUBLIC FILE_SET CXX_MODULES FILES modules/cmdline.cpp)
target_compile_options(cmdline PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(evdev STATIC)
target_sources(evdev PUBLIC FILE_SET CXX_MODULES FILES modules/evdev.cpp)
target_compile_options(evdev PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(nt STATIC)
target_sources(nt PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
target_compile_options(nt PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(ntdll_mod STATIC)
target_sources(ntdll_mod PUBLIC FILE_SET CXX_MODULES FILES modules/ntdll.cpp)
target_compile_options(ntdll_mod PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ntdll_mod nt)

add_library(pe STATIC)
target_sources(pe PUBLIC FILE_SET CXX_MODULES FILES modules/pe.cpp)
target_compile_options(pe PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(loaderblock STATIC)
target_sources(loaderblock PUBLIC FILE_SET CXX_MODULES FILES modules/loaderblock.cpp)
target_compile_options(loaderblock PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(loaderblock gnu-efi)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_link_libraries(loaderblock format)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    target_link_libraries(loaderblock format_kernel)
endif()

add_library(kernelbase_mod STATIC)
target_sources(kernelbase_mod PUBLIC FILE_SET CXX_MODULES FILES modules/kernelbase.cpp)
target_compile_options(kernelbase_mod PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(kernelbase_mod nt)

add_library(ucrtbase_mod STATIC)
target_sources(ucrtbase_mod PUBLIC FILE_SET CXX_MODULES FILES modules/ucrtbase.cpp)
target_compile_options(ucrtbase_mod PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(crt STATIC)
target_sources(crt PUBLIC FILE_SET CXX_MODULES FILES modules/crt.cpp)
target_compile_options(crt PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    target_sources(crt PUBLIC modules/crt.S)
endif()

add_library(pagesize STATIC)
target_sources(pagesize PUBLIC FILE_SET CXX_MODULES FILES modules/pagesize.cpp)
target_compile_options(pagesize PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(timepage STATIC)
target_sources(timepage PUBLIC FILE_SET CXX_MODULES FILES modules/timepage.cpp)
target_compile_options(timepage PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(timepage pagesize)

add_library(getopt STATIC)
target_sources(getopt PUBLIC FILE_SET CXX_MODULES FILES modules/getopt.cpp)
target_compile_options(getopt PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(getopt format)
target_compile_definitions(getopt PUBLIC -D_LIBCPP_VERBOSE_ABORT=)

add_library(moxnox_ioctls STATIC)
target_sources(moxnox_ioctls PUBLIC FILE_SET CXX_MODULES FILES modules/moxnox_ioctls.cpp)
target_compile_options(moxnox_ioctls PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(moxnox_ioctls nt)

add_library(env STATIC)
target_sources(env PUBLIC FILE_SET CXX_MODULES FILES modules/env.cpp)
target_compile_options(env PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(env nt)

add_library(print STATIC)
target_sources(print PUBLIC FILE_SET CXX_MODULES FILES modules/print.cpp)
target_compile_options(print PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(print nt format moxnox)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_library(aarch64 STATIC)
    target_sources(aarch64 PUBLIC FILE_SET CXX_MODULES FILES modules/aarch64.cpp)
    target_compile_options(aarch64 PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(aarch64 format)
endif()

include(check-git.cmake)
check_git_setup()

add_subdirectory(kernel)
add_subdirectory(ntdll)
add_subdirectory(init)
add_subdirectory(libmoxnox)
add_subdirectory(kernelbase)
add_subdirectory(ucrtbase)
add_subdirectory(usercon)
add_subdirectory(shell)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_subdirectory(bootloader/aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    add_subdirectory(bootloader/x86)
endif()

set(FREETYPE_FILES freetype/src/autofit/autofit.c
    freetype/src/base/ftbase.c
    freetype/src/base/ftbbox.c
    freetype/src/base/ftbdf.c
    freetype/src/base/ftbitmap.c
    freetype/src/base/ftcid.c
    freetype/src/base/ftfstype.c
    freetype/src/base/ftgasp.c
    freetype/src/base/ftglyph.c
    freetype/src/base/ftgxval.c
    freetype/src/base/ftinit.c
    freetype/src/base/ftmm.c
    freetype/src/base/ftotval.c
    freetype/src/base/ftpatent.c
    freetype/src/base/ftpfr.c
    freetype/src/base/ftstroke.c
    freetype/src/base/ftsynth.c
    freetype/src/base/fttype1.c
    freetype/src/base/ftwinfnt.c
    freetype/src/bzip2/ftbzip2.c
    freetype/src/cache/ftcache.c
    freetype/src/gzip/ftgzip.c
    freetype/src/lzw/ftlzw.c
    freetype/src/pcf/pcf.c
    freetype/src/pfr/pfr.c
    freetype/src/pshinter/pshinter.c
    freetype/src/raster/raster.c
    freetype/src/sfnt/sfnt.c
    freetype/src/smooth/smooth.c
    freetype/src/truetype/truetype.c
    freetype/src/winfonts/winfnt.c)

add_library(freetype_kernel STATIC ${FREETYPE_FILES})
target_sources(freetype_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/freetype.cpp)

target_compile_definitions(freetype_kernel PRIVATE -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
target_compile_definitions(freetype_kernel PRIVATE -DFT_CONFIG_MODULES_H=\"${CMAKE_SOURCE_DIR}/modules/ftmodule.h\")
target_compile_definitions(freetype_kernel PRIVATE -DFT_CONFIG_STANDARD_LIBRARY_H=\"${CMAKE_SOURCE_DIR}/modules/ftstdlib.h\")
target_compile_options(freetype_kernel PRIVATE -mno-stack-arg-probe ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_include_directories(freetype_kernel PRIVATE modules)
target_include_directories(freetype_kernel PRIVATE freetype/include)

# undo freetype's dllexport markings
add_custom_command(
    TARGET freetype_kernel
    POST_BUILD
    COMMAND objcopy --remove-section .drectve "${CMAKE_CURRENT_BINARY_DIR}/libfreetype_kernel.a"
)

add_library(freetype SHARED ${FREETYPE_FILES} ${CMAKE_SOURCE_DIR}/ftshim.cpp)

add_library(freetype_module STATIC)
target_sources(freetype_module PUBLIC FILE_SET CXX_MODULES FILES modules/freetype.cpp)
target_include_directories(freetype_module PRIVATE freetype/include)
target_compile_options(freetype_module PRIVATE -mno-stack-arg-probe ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_link_libraries(freetype_module PUBLIC freetype)

target_compile_definitions(freetype PRIVATE -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT -DFT_CONFIG_OPTION_USE_PNG)
target_compile_definitions(freetype PRIVATE -DFT_CONFIG_MODULES_H=\"${CMAKE_SOURCE_DIR}/modules/ftmodule.h\")
target_compile_definitions(freetype PRIVATE -DFT_CONFIG_STANDARD_LIBRARY_H=\"${CMAKE_SOURCE_DIR}/modules/ftstdlib.h\")
target_compile_options(freetype PRIVATE -mno-stack-arg-probe ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_include_directories(freetype PRIVATE modules)
target_include_directories(freetype PRIVATE freetype/include)
target_link_options(freetype PRIVATE -nostartfiles -nostdlib)
target_link_libraries(freetype PRIVATE moxnox)
target_link_libraries(freetype PRIVATE libpng)

install(TARGETS freetype DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)

add_library(harfbuzz SHARED
    harfbuzz/src/hb-aat-layout.cc
    harfbuzz/src/hb-blob.cc
    harfbuzz/src/hb-buffer.cc
    harfbuzz/src/hb-common.cc
    harfbuzz/src/hb-face-builder.cc
    harfbuzz/src/hb-face.cc
    harfbuzz/src/hb-fallback-shape.cc
    harfbuzz/src/hb-font.cc
    harfbuzz/src/hb-ft.cc
    harfbuzz/src/hb-number.cc
    harfbuzz/src/hb-ot-face.cc
    harfbuzz/src/hb-ot-font.cc
    harfbuzz/src/hb-ot-layout.cc
    harfbuzz/src/hb-ot-map.cc
    harfbuzz/src/hb-ot-metrics.cc
    harfbuzz/src/hb-ot-shape.cc
    harfbuzz/src/hb-ot-shape-fallback.cc
    harfbuzz/src/hb-ot-shape-normalize.cc
    harfbuzz/src/hb-ot-shaper-arabic.cc
    harfbuzz/src/hb-ot-shaper-default.cc
    harfbuzz/src/hb-ot-shaper-hangul.cc
    harfbuzz/src/hb-ot-shaper-hebrew.cc
    harfbuzz/src/hb-ot-shaper-indic.cc
    harfbuzz/src/hb-ot-shaper-indic-table.cc
    harfbuzz/src/hb-ot-shaper-khmer.cc
    harfbuzz/src/hb-ot-shaper-myanmar.cc
    harfbuzz/src/hb-ot-shaper-syllabic.cc
    harfbuzz/src/hb-ot-shaper-thai.cc
    harfbuzz/src/hb-ot-shaper-use.cc
    harfbuzz/src/hb-ot-shaper-vowel-constraints.cc
    harfbuzz/src/hb-ot-tag.cc
    harfbuzz/src/hb-set.cc
    harfbuzz/src/hb-shape.cc
    harfbuzz/src/hb-shape-plan.cc
    harfbuzz/src/hb-shaper.cc
    harfbuzz/src/hb-static.cc
    harfbuzz/src/hb-ucd.cc
    harfbuzz/src/hb-unicode.cc
    ${CMAKE_SOURCE_DIR}/hbshim.cpp)
target_sources(harfbuzz PUBLIC FILE_SET CXX_MODULES FILES modules/harfbuzz.cpp)

target_compile_definitions(harfbuzz PRIVATE -DHAVE_FREETYPE -DHB_LEAN -DHB_NO_AAT -DHB_NO_MT)
target_compile_options(harfbuzz PRIVATE -mno-stack-arg-probe -Wa,-mbig-obj -fno-exceptions -fno-rtti ${GENERAL_CFLAGS} -w)
target_compile_options(harfbuzz PRIVATE -include ${CMAKE_SOURCE_DIR}/hbshim.h)
target_include_directories(harfbuzz PRIVATE harfbuzz/src)
target_include_directories(harfbuzz PRIVATE freetype/include)
target_link_libraries(harfbuzz PRIVATE freetype)
target_link_options(harfbuzz PRIVATE -nostartfiles -nostdlib)
target_link_libraries(harfbuzz PRIVATE moxnox -lgcc)

install(TARGETS harfbuzz DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)

add_library(acpica STATIC
    acpica/source/components/dispatcher/dsargs.c
    acpica/source/components/dispatcher/dscontrol.c
    acpica/source/components/dispatcher/dsdebug.c
    acpica/source/components/dispatcher/dsfield.c
    acpica/source/components/dispatcher/dsinit.c
    acpica/source/components/dispatcher/dsmethod.c
    acpica/source/components/dispatcher/dsmthdat.c
    acpica/source/components/dispatcher/dsobject.c
    acpica/source/components/dispatcher/dsopcode.c
    acpica/source/components/dispatcher/dspkginit.c
    acpica/source/components/dispatcher/dsutils.c
    acpica/source/components/dispatcher/dswexec.c
    acpica/source/components/dispatcher/dswload2.c
    acpica/source/components/dispatcher/dswload.c
    acpica/source/components/dispatcher/dswscope.c
    acpica/source/components/dispatcher/dswstate.c
    acpica/source/components/events/evevent.c
    acpica/source/components/events/evglock.c
    acpica/source/components/events/evgpeblk.c
    acpica/source/components/events/evgpe.c
    acpica/source/components/events/evgpeinit.c
    acpica/source/components/events/evgpeutil.c
    acpica/source/components/events/evhandler.c
    acpica/source/components/events/evmisc.c
    acpica/source/components/events/evregion.c
    acpica/source/components/events/evrgnini.c
    acpica/source/components/events/evsci.c
    acpica/source/components/events/evxface.c
    acpica/source/components/events/evxfevnt.c
    acpica/source/components/events/evxfgpe.c
    acpica/source/components/events/evxfregn.c
    acpica/source/components/executer/exconcat.c
    acpica/source/components/executer/exconfig.c
    acpica/source/components/executer/exconvrt.c
    acpica/source/components/executer/excreate.c
    acpica/source/components/executer/exdebug.c
    acpica/source/components/executer/exdump.c
    acpica/source/components/executer/exfield.c
    acpica/source/components/executer/exfldio.c
    acpica/source/components/executer/exmisc.c
    acpica/source/components/executer/exmutex.c
    acpica/source/components/executer/exnames.c
    acpica/source/components/executer/exoparg1.c
    acpica/source/components/executer/exoparg2.c
    acpica/source/components/executer/exoparg3.c
    acpica/source/components/executer/exoparg6.c
    acpica/source/components/executer/exprep.c
    acpica/source/components/executer/exregion.c
    acpica/source/components/executer/exresnte.c
    acpica/source/components/executer/exresolv.c
    acpica/source/components/executer/exresop.c
    acpica/source/components/executer/exserial.c
    acpica/source/components/executer/exstore.c
    acpica/source/components/executer/exstoren.c
    acpica/source/components/executer/exstorob.c
    acpica/source/components/executer/exsystem.c
    acpica/source/components/executer/extrace.c
    acpica/source/components/executer/exutils.c
    acpica/source/components/hardware/hwacpi.c
    acpica/source/components/hardware/hwesleep.c
    acpica/source/components/hardware/hwgpe.c
    acpica/source/components/hardware/hwpci.c
    acpica/source/components/hardware/hwregs.c
    acpica/source/components/hardware/hwsleep.c
    acpica/source/components/hardware/hwtimer.c
    acpica/source/components/hardware/hwvalid.c
    acpica/source/components/hardware/hwxface.c
    acpica/source/components/hardware/hwxfsleep.c
    acpica/source/components/namespace/nsaccess.c
    acpica/source/components/namespace/nsalloc.c
    acpica/source/components/namespace/nsarguments.c
    acpica/source/components/namespace/nsconvert.c
    acpica/source/components/namespace/nsdump.c
    acpica/source/components/namespace/nsdumpdv.c
    acpica/source/components/namespace/nseval.c
    acpica/source/components/namespace/nsinit.c
    acpica/source/components/namespace/nsload.c
    acpica/source/components/namespace/nsnames.c
    acpica/source/components/namespace/nsobject.c
    acpica/source/components/namespace/nsparse.c
    acpica/source/components/namespace/nspredef.c
    acpica/source/components/namespace/nsprepkg.c
    acpica/source/components/namespace/nsrepair2.c
    acpica/source/components/namespace/nsrepair.c
    acpica/source/components/namespace/nssearch.c
    acpica/source/components/namespace/nsutils.c
    acpica/source/components/namespace/nswalk.c
    acpica/source/components/namespace/nsxfeval.c
    acpica/source/components/namespace/nsxfname.c
    acpica/source/components/namespace/nsxfobj.c
    acpica/source/components/parser/psargs.c
    acpica/source/components/parser/psloop.c
    acpica/source/components/parser/psobject.c
    acpica/source/components/parser/psopcode.c
    acpica/source/components/parser/psopinfo.c
    acpica/source/components/parser/psparse.c
    acpica/source/components/parser/psscope.c
    acpica/source/components/parser/pstree.c
    acpica/source/components/parser/psutils.c
    acpica/source/components/parser/pswalk.c
    acpica/source/components/parser/psxface.c
    acpica/source/components/resources/rsaddr.c
    acpica/source/components/resources/rscalc.c
    acpica/source/components/resources/rscreate.c
    acpica/source/components/resources/rsdumpinfo.c
    acpica/source/components/resources/rsinfo.c
    acpica/source/components/resources/rsio.c
    acpica/source/components/resources/rsirq.c
    acpica/source/components/resources/rslist.c
    acpica/source/components/resources/rsmemory.c
    acpica/source/components/resources/rsmisc.c
    acpica/source/components/resources/rsserial.c
    acpica/source/components/resources/rsutils.c
    acpica/source/components/resources/rsxface.c
    acpica/source/components/tables/tbdata.c
    acpica/source/components/tables/tbfadt.c
    acpica/source/components/tables/tbfind.c
    acpica/source/components/tables/tbinstal.c
    acpica/source/components/tables/tbprint.c
    acpica/source/components/tables/tbutils.c
    acpica/source/components/tables/tbxface.c
    acpica/source/components/tables/tbxfload.c
    acpica/source/components/tables/tbxfroot.c
    acpica/source/components/utilities/utaddress.c
    acpica/source/components/utilities/utalloc.c
    acpica/source/components/utilities/utascii.c
    acpica/source/components/utilities/utbuffer.c
    acpica/source/components/utilities/utcache.c
    acpica/source/components/utilities/utcksum.c
    acpica/source/components/utilities/utclib.c
    acpica/source/components/utilities/utcopy.c
    acpica/source/components/utilities/utdebug.c
    acpica/source/components/utilities/utdecode.c
    acpica/source/components/utilities/utdelete.c
    acpica/source/components/utilities/uterror.c
    acpica/source/components/utilities/uteval.c
    acpica/source/components/utilities/utexcep.c
    acpica/source/components/utilities/utglobal.c
    acpica/source/components/utilities/uthex.c
    acpica/source/components/utilities/utids.c
    acpica/source/components/utilities/utinit.c
    acpica/source/components/utilities/utlock.c
    acpica/source/components/utilities/utmath.c
    acpica/source/components/utilities/utmisc.c
    acpica/source/components/utilities/utmutex.c
    acpica/source/components/utilities/utnonansi.c
    acpica/source/components/utilities/utobject.c
    acpica/source/components/utilities/utosi.c
    acpica/source/components/utilities/utownerid.c
    acpica/source/components/utilities/utpredef.c
    acpica/source/components/utilities/utresdecode.c
    acpica/source/components/utilities/utresrc.c
    acpica/source/components/utilities/utstate.c
    acpica/source/components/utilities/utstring.c
    acpica/source/components/utilities/utstrsuppt.c
    acpica/source/components/utilities/utstrtoul64.c
    acpica/source/components/utilities/uttrack.c
    acpica/source/components/utilities/utuuid.c
    acpica/source/components/utilities/utxface.c
    acpica/source/components/utilities/utxferror.c
    acpica/source/components/utilities/utxfinit.c
    acpica/source/components/utilities/utxfmutex.c
)
target_sources(acpica PUBLIC FILE_SET CXX_MODULES FILES modules/acpica.cpp)

target_include_directories(acpica PRIVATE acpica/source/include)
target_compile_definitions(acpica PRIVATE -D_LINUX -DACPI_NO_ERROR_MESSAGES -DACPI_USE_LOCAL_CACHE -DACPI_USE_SYSTEM_INTTYPES)
target_compile_options(acpica PRIVATE $<$<COMPILE_LANGUAGE:C>:-include ${CMAKE_SOURCE_DIR}/acpica-shim.h>)
target_compile_options(acpica PRIVATE ${GENERAL_CFLAGS} -Wno-overflow -Wno-shift-count-overflow -Wno-constant-conversion -Wno-write-strings)
target_compile_options(acpica PRIVATE $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-Wno-global-module>)
target_compile_options(acpica PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)

add_library(zlib SHARED
    zlib/adler32.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/zutil.c
    ${CMAKE_SOURCE_DIR}/zlibshim.cpp
)

target_include_directories(zlib PUBLIC zlib)
target_compile_options(zlib PRIVATE ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_link_options(zlib PUBLIC -nostartfiles -nostdlib)
target_link_libraries(zlib PRIVATE moxnox)

install(TARGETS zlib DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)

add_library(libpng SHARED
    libpng/png.c
    libpng/pngerror.c
    libpng/pngget.c
    libpng/pngmem.c
    libpng/pngpread.c
    libpng/pngread.c
    libpng/pngrio.c
    libpng/pngrtran.c
    libpng/pngrutil.c
    libpng/pngset.c
    libpng/pngtrans.c
    libpng/pngwio.c
    libpng/pngwrite.c
    libpng/pngwtran.c
    libpng/pngwutil.c
    ${CMAKE_SOURCE_DIR}/libpngshim.cpp
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_sources(libpng PUBLIC
        libpng/arm/arm_init.c
        libpng/arm/filter_neon_intrinsics.c
        libpng/arm/palette_neon_intrinsics.c
    )
endif()

target_include_directories(libpng PUBLIC ${CMAKE_SOURCE_DIR} libpng)
target_compile_options(libpng PRIVATE ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_link_libraries(libpng PUBLIC zlib)
target_link_libraries(libpng PRIVATE moxnox)

install(TARGETS libpng DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)

install(SCRIPT initrd.cmake)
