cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

include_directories(include)

add_subdirectory(kernel)
add_subdirectory(kernel2)
add_subdirectory(efi)
add_subdirectory(init)
add_subdirectory(shell)
add_subdirectory(ntdll)

add_library(freetype freetype/src/autofit/autofit.c
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

target_compile_definitions(freetype PRIVATE -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
target_compile_options(freetype PRIVATE -mno-stack-arg-probe)
target_include_directories(freetype PRIVATE freetype/include)
