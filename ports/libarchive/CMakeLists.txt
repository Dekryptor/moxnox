set(BUILD_SHARED_LIBS ON)
set(ENABLE_TEST OFF)
set(ENABLE_WERROR OFF)
set(ENABLE_INSTALL ON)

set(ENABLE_TAR_SHARED ON)
set(ENABLE_CPIO_SHARED ON)
set(ENABLE_CAT_SHARED ON)
set(ENABLE_UNZIP_SHARED ON)

set(ENABLE_LZ4 OFF)
set(ENABLE_LZO OFF)
set(ENABLE_LIBB2 OFF)
set(ENABLE_LIBXML2 OFF)
set(ENABLE_PCREPOSIX OFF)
set(ENABLE_PCRE2POSIX OFF)
set(ENABLE_LIBGCC OFF)
set(ENABLE_CNG OFF)
set(ENABLE_MBEDTLS OFF)
set(ENABLE_NETTLE OFF)
set(ENABLE_WIN32_XMLLITE OFF)

set(ENABLE_ACL OFF)
set(ENABLE_XATTR OFF)

set(ENABLE_ZLIB ON)
set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/zlib/repo)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libzlib.dll.a)

set(ENABLE_BZip2 ON)
set(BZIP2_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/bzip2/repo)
set(BZIP2_LIBRARIES ${CMAKE_BINARY_DIR}/ports/bzip2/libbz2.dll.a)

set(ENABLE_LZMA ON)
set(LIBLZMA_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/xz/repo/src/liblzma/api)
set(LIBLZMA_LIBRARY ${CMAKE_BINARY_DIR}/ports/xz/repo/liblzma.dll.a)

set(ENABLE_ZSTD ON)
set(ZSTD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/zstd/repo/lib)
set(ZSTD_LIBRARY ${CMAKE_BINARY_DIR}/ports/zstd/libzstd.dll.a)

set(ENABLE_OPENSSL ON)
set(OPENSSL_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/openssl/gen/include)
set(OPENSSL_SSL_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/libssl.dll.a)
set(OPENSSL_CRYPTO_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/liblibcrypto.dll.a)
file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

set(ENABLE_ICONV ON)
set(ICONV_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/libiconv/gen/include)
set(LIBICONV_PATH ${CMAKE_BINARY_DIR}/ports/libiconv/libiconv-2.dll.a)
set(LIBCHARSET_PATH ${LIBICONV_PATH} CACHE FILEPATH "" FORCE)
file(MAKE_DIRECTORY ${ICONV_INCLUDE_DIR})

set(ENABLE_EXPAT ON)
set(EXPAT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/expat/repo/expat/lib)
set(EXPAT_LIBRARY ${CMAKE_BINARY_DIR}/ports/expat/repo/expat/libexpat.dll.a)

set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(HAVE__FSEEKI64 0 CACHE INTERNAL "")

set(HAVE_LINUX_FIEMAP_H 0 CACHE INTERNAL "")
set(HAVE_LINUX_FS_H 0 CACHE INTERNAL "")
set(HAVE_LINUX_MAGIC_H 0 CACHE INTERNAL "")
set(HAVE_LINUX_TYPES_H 0 CACHE INTERNAL "")

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
add_subdirectory(repo)

set(LIBARCHIVE_CFLAGS
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-parameter
)

target_compile_options(archive PRIVATE ${LIBARCHIVE_CFLAGS} -fvisibility=default)

foreach(_target archive archive_static)
    target_include_directories(${_target} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/ports/openssl/repo/include)
endforeach()

foreach(_prog bsdtar bsdcat bsdcpio bsdunzip)
    target_compile_options(${_prog} PRIVATE ${LIBARCHIVE_CFLAGS})
endforeach()

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies archive zlib bzip2 liblzma zstd libcrypto libiconv expat)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies archive_static zlib bzip2 liblzma zstd libcrypto libiconv expat)

add_custom_target(libarchive)
add_dependencies(libarchive
    archive
    bsdcat
    bsdcpio
    bsdtar
    bsdunzip
)
