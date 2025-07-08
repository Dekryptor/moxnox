cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

include_directories(include)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

add_library(format STATIC)
target_sources(format PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(format_sse STATIC)
target_sources(format_sse PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format_sse PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(dwarf STATIC)
target_sources(dwarf PUBLIC FILE_SET CXX_MODULES FILES modules/dwarf.cpp)
target_compile_options(dwarf PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)
target_link_libraries(dwarf format)

add_library(unwind STATIC)
target_sources(unwind PUBLIC FILE_SET CXX_MODULES FILES modules/unwind.cpp)
target_compile_options(unwind PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(list STATIC)
target_sources(list PUBLIC FILE_SET CXX_MODULES FILES modules/list.cpp)
target_compile_options(list PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(list_sse STATIC)
target_sources(list_sse PUBLIC FILE_SET CXX_MODULES FILES modules/list.cpp)
target_compile_options(list_sse PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(unicode STATIC)
target_sources(unicode PUBLIC FILE_SET CXX_MODULES FILES modules/unicode.cpp)
target_compile_options(unicode PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(rbtree STATIC)
target_sources(rbtree PUBLIC FILE_SET CXX_MODULES FILES modules/rbtree.cpp)
target_compile_options(rbtree PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(rbtree_sse STATIC)
target_sources(rbtree_sse PUBLIC FILE_SET CXX_MODULES FILES modules/rbtree.cpp)
target_compile_options(rbtree_sse PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(unique_handle STATIC)
target_sources(unique_handle PUBLIC FILE_SET CXX_MODULES FILES modules/unique_handle.cpp)
target_compile_options(unique_handle PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(vt100 STATIC)
target_sources(vt100 PUBLIC FILE_SET CXX_MODULES FILES modules/vt100.cpp)
target_compile_options(vt100 PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_library(vt100_sse STATIC)
target_sources(vt100_sse PUBLIC FILE_SET CXX_MODULES FILES modules/vt100.cpp)
target_compile_options(vt100_sse PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)

add_subdirectory(kernel)
add_subdirectory(bootloader)
add_subdirectory(init)
add_subdirectory(shell)
add_subdirectory(ntdll)
add_subdirectory(usercon)

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

add_library(freetype-static STATIC ${FREETYPE_FILES})

target_compile_definitions(freetype-static PRIVATE -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
target_compile_definitions(freetype-static PRIVATE -DFT_CONFIG_MODULES_H=\"${CMAKE_SOURCE_DIR}/include/ftmodule.h\")
target_compile_definitions(freetype-static PRIVATE -DFT_CONFIG_STANDARD_LIBRARY_H=\"${CMAKE_SOURCE_DIR}/include/ftstdlib.h\")
target_compile_options(freetype-static PRIVATE -mno-stack-arg-probe -gdwarf-5)
target_include_directories(freetype-static PRIVATE include)
target_include_directories(freetype-static PUBLIC freetype/include)

add_library(freetype SHARED ${FREETYPE_FILES} ftshim.cpp)

target_compile_definitions(freetype PRIVATE -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
target_compile_definitions(freetype PRIVATE -DFT_CONFIG_MODULES_H=\"${CMAKE_SOURCE_DIR}/include/ftmodule.h\")
target_compile_definitions(freetype PRIVATE -DFT_CONFIG_STANDARD_LIBRARY_H=\"${CMAKE_SOURCE_DIR}/include/ftstdlib.h\")
target_compile_options(freetype PRIVATE -mno-stack-arg-probe -gdwarf-5 -masm=intel)
target_include_directories(freetype PRIVATE include)
target_include_directories(freetype PUBLIC freetype/include)
target_link_options(freetype PUBLIC -nostartfiles -nostdlib)
target_link_libraries(freetype ntdll)

install(TARGETS freetype DESTINATION ${CMAKE_INSTALL_PREFIX}
    ARCHIVE EXCLUDE_FROM_ALL
)

add_library(harfbuzz harfbuzz/src/hb-aat-layout.cc
    harfbuzz/src/hb-aat-map.cc
    harfbuzz/src/hb-blob.cc
    harfbuzz/src/hb-buffer.cc
    harfbuzz/src/hb-buffer-serialize.cc
    harfbuzz/src/hb-buffer-verify.cc
    harfbuzz/src/hb-cairo.cc
    harfbuzz/src/hb-cairo-utils.cc
    harfbuzz/src/hb-common.cc
    harfbuzz/src/hb-coretext.cc
    harfbuzz/src/hb-coretext-font.cc
    harfbuzz/src/hb-coretext-shape.cc
    harfbuzz/src/hb-directwrite.cc
    harfbuzz/src/hb-directwrite-font.cc
    harfbuzz/src/hb-directwrite-shape.cc
    harfbuzz/src/hb-draw.cc
    harfbuzz/src/hb-face-builder.cc
    harfbuzz/src/hb-face.cc
    harfbuzz/src/hb-fallback-shape.cc
    harfbuzz/src/hb-font.cc
    harfbuzz/src/hb-ft.cc
    harfbuzz/src/hb-gdi.cc
    harfbuzz/src/hb-glib.cc
    harfbuzz/src/hb-gobject-structs.cc
    harfbuzz/src/hb-graphite2.cc
    harfbuzz/src/hb-harfruzz.cc
    harfbuzz/src/hb-icu.cc
    harfbuzz/src/hb-map.cc
    harfbuzz/src/hb-number.cc
    harfbuzz/src/hb-ot-cff1-table.cc
    harfbuzz/src/hb-ot-cff2-table.cc
    harfbuzz/src/hb-ot-color.cc
    harfbuzz/src/hb-ot-face.cc
    harfbuzz/src/hb-ot-font.cc
    harfbuzz/src/hb-ot-layout.cc
    harfbuzz/src/hb-ot-map.cc
    harfbuzz/src/hb-ot-math.cc
    harfbuzz/src/hb-ot-meta.cc
    harfbuzz/src/hb-ot-metrics.cc
    harfbuzz/src/hb-ot-name.cc
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
    harfbuzz/src/hb-ot-var.cc
    harfbuzz/src/hb-outline.cc
    harfbuzz/src/hb-paint-bounded.cc
    harfbuzz/src/hb-paint.cc
    harfbuzz/src/hb-paint-extents.cc
    harfbuzz/src/hb-set.cc
    harfbuzz/src/hb-shape.cc
    harfbuzz/src/hb-shape-plan.cc
    harfbuzz/src/hb-shaper.cc
    harfbuzz/src/hb-static.cc
    harfbuzz/src/hb-style.cc
    harfbuzz/src/hb-subset.cc
    harfbuzz/src/hb-subset-cff1.cc
    harfbuzz/src/hb-subset-cff2.cc
    harfbuzz/src/hb-subset-cff-common.cc
    harfbuzz/src/hb-subset-input.cc
    harfbuzz/src/hb-subset-instancer-iup.cc
    harfbuzz/src/hb-subset-instancer-solver.cc
    harfbuzz/src/hb-subset-plan.cc
    harfbuzz/src/hb-subset-plan-layout.cc
    harfbuzz/src/hb-subset-plan-var.cc
    harfbuzz/src/hb-subset-serialize.cc
    harfbuzz/src/hb-ucd.cc
    harfbuzz/src/hb-unicode.cc
    harfbuzz/src/hb-uniscribe.cc
    harfbuzz/src/hb-wasm-api.cc
    harfbuzz/src/hb-wasm-shape.cc)

target_compile_definitions(harfbuzz PRIVATE -DHAVE_FREETYPE -DHB_LEAN -DHB_NO_AAT -DHB_NO_OT_SHAPE -DHB_NO_OT_LAYOUT -DHB_NO_MT)
target_compile_options(harfbuzz PRIVATE -mno-stack-arg-probe -Wa,-mbig-obj -fno-exceptions -gdwarf-5)
target_include_directories(harfbuzz PUBLIC harfbuzz/src)
target_link_libraries(harfbuzz freetype-static)

add_library(acpica
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

target_include_directories(acpica PRIVATE acpica/source/include)
target_compile_definitions(acpica PRIVATE -D_LINUX -DACPI_NO_ERROR_MESSAGES -DACPI_USE_LOCAL_CACHE -DACPI_USE_SYSTEM_INTTYPES)
target_compile_options(acpica PRIVATE -include ${CMAKE_SOURCE_DIR}/acpica-shim.h)
target_compile_options(acpica PRIVATE -Wno-overflow -Wno-shift-count-overflow -Wno-constant-conversion -gdwarf-5)

add_library(zlib
    zlib/adler32.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/zutil.c
)

target_include_directories(zlib PUBLIC zlib)
target_compile_options(zlib PRIVATE -gdwarf-5)

add_library(libpng
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
)

target_include_directories(libpng PUBLIC ${CMAKE_SOURCE_DIR} libpng)
target_compile_options(libpng PRIVATE -gdwarf-5)
target_link_libraries(libpng zlib)

install(SCRIPT initrd.cmake)
