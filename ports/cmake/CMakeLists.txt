set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(BUILD_TESTING OFF)
set(CMake_ENABLE_DEBUGGER OFF)
set(BUILD_CursesDialog OFF)
set(CMAKE_USE_OPENSSL OFF)

set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)

set(CMAKE_USE_SYSTEM_LIBRARY_CURL ON)
set(CMAKE_USE_SYSTEM_LIBRARY_EXPAT ON)
set(CMAKE_USE_SYSTEM_LIBRARY_JSONCPP ON)
set(CMAKE_USE_SYSTEM_LIBRARY_LIBRHASH ON)
set(CMAKE_USE_SYSTEM_LIBRARY_LIBUV ON)
set(CMAKE_USE_SYSTEM_LIBRARY_ZLIB ON)
set(CMAKE_USE_SYSTEM_LIBRARY_LIBARCHIVE ON)

set(CURL_LIBRARY ${CMAKE_BINARY_DIR}/ports/curl/repo/lib/libcurl.dll.a)
set(CURL_LIBRARY_RELEASE ${CMAKE_BINARY_DIR}/ports/curl/repo/lib/libcurl.dll.a)
set(CURL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/curl/repo/include)
set(EXPAT_LIBRARY ${CMAKE_BINARY_DIR}/ports/expat/repo/expat/libexpat.dll.a)
set(EXPAT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/expat/repo/expat/lib)
set(JsonCpp_LIBRARY ${CMAKE_BINARY_DIR}/ports/jsoncpp/repo/src/lib_json/libjsoncpp.dll.a)
set(JsonCpp_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/jsoncpp/repo/include)
set(LibRHash_LIBRARY ${CMAKE_BINARY_DIR}/ports/librhash/liblibrhash.dll.a)
set(LibRHash_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/librhash/repo/librhash)
set(LibUV_LIBRARY ${CMAKE_BINARY_DIR}/ports/libuv/repo/libuv.dll.a)
set(LibUV_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/libuv/repo/include)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libzlib.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/zlib/repo)
set(LibArchive_LIBRARY ${CMAKE_BINARY_DIR}/ports/libarchive/libarchive.dll.a)
set(LibArchive_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/libarchive/repo/libarchive)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE) # cmake is very chatty
add_subdirectory(repo)

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies CMakeLib libcurl_shared expat jsoncpp_lib librhash uv zlib archive)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies CTestLib libcurl_shared expat jsoncpp_lib librhash uv zlib archive)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies CPackLib libcurl_shared expat jsoncpp_lib librhash uv zlib archive)
