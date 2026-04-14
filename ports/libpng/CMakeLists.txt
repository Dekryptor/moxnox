add_library(libpng SHARED
    repo/png.c
    repo/pngerror.c
    repo/pngget.c
    repo/pngmem.c
    repo/pngpread.c
    repo/pngread.c
    repo/pngrio.c
    repo/pngrtran.c
    repo/pngrutil.c
    repo/pngset.c
    repo/pngtrans.c
    repo/pngwio.c
    repo/pngwrite.c
    repo/pngwtran.c
    repo/pngwutil.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_sources(libpng PRIVATE
        repo/arm/arm_init.c
        repo/arm/filter_neon_intrinsics.c
        repo/arm/palette_neon_intrinsics.c
    )
endif()

target_include_directories(libpng PUBLIC repo ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(libpng PRIVATE ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -Wno-sign-conversion)
target_link_libraries(libpng PUBLIC zlib)

install(TARGETS libpng DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)
