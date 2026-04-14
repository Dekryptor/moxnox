add_library(zlib SHARED
    repo/adler32.c
    repo/crc32.c
    repo/deflate.c
    repo/inffast.c
    repo/inflate.c
    repo/inftrees.c
    repo/trees.c
    repo/zutil.c
)

target_include_directories(zlib PUBLIC repo)
target_compile_options(zlib PRIVATE ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)

install(TARGETS zlib DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)
