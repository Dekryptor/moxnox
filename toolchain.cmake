if(NOT DEFINED MOXNOX_BUILD_ROOT)
    set(MOXNOX_BUILD_ROOT ${CMAKE_BINARY_DIR} CACHE INTERNAL "")
endif()
if(NOT DEFINED MOXNOX_SOURCE_ROOT)
    set(MOXNOX_SOURCE_ROOT ${CMAKE_SOURCE_DIR} CACHE INTERNAL "")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MOXNOX_BUILD_ROOT MOXNOX_SOURCE_ROOT)

if(NOT MOXNOX_BUILD_ROOT STREQUAL CMAKE_BINARY_DIR)
    if(LIBCXX_HEADERS)
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-isystem${LIBCXX_HEADERS}>")
    endif()
    add_compile_options(-isystem "${MOXNOX_SOURCE_ROOT}/include")
endif()

if (USE_COMPILER_RT)
    set(LIBGCC "-lclang_rt.builtins")
else()
    set(LIBGCC "-lgcc")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(LIBSTDCXX "-Wl,-Bstatic -lc++ -lc++abi -Wl,-Bdynamic")
    set(LIBEH "-Wl,-Bstatic -lunwind -Wl,-Bdynamic")
else()
    if (USE_COMPILER_RT)
        set(LIBSTDCXX "-Wl,-Bstatic -lc++ -lc++abi -Wl,-Bdynamic")
        set(LIBEH "-Wl,-Bstatic -lunwind -Wl,-Bdynamic")
    else()
        set(LIBSTDCXX "-Wl,-Bstatic -lstdc++ -Wl,-Bdynamic")
        set(LIBEH "-lgcc_eh")
    endif()
endif()

if (STATIC_LIBCXX)
    set(LIBSTDCXX_EXE "-Wl,-Bstatic -lc++ -Wl,--whole-archive -lc++abi -Wl,--no-whole-archive -Wl,-Bdynamic")
else()
    set(LIBSTDCXX_EXE "${LIBSTDCXX}")
endif()

if (USE_COMPILER_RT)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} --target=${CLANG_TARGET_TRIPLE} -print-runtime-dir
        OUTPUT_VARIABLE CLANG_RUNTIME_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (IS_DIRECTORY "${CLANG_RUNTIME_DIR}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CLANG_RUNTIME_DIR}")
        set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} -L${CLANG_RUNTIME_DIR}")
    endif()
endif()

set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> ${CMAKE_GNULD_IMAGE_VERSION} <LINK_LIBRARIES> ${LIBSTDCXX_EXE} ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${LIBEH} ${LIBGCC} ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingwex.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libucrtbase_static.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libmoxnox.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libucrtbase.a")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> ${CMAKE_GNULD_IMAGE_VERSION} <LINK_LIBRARIES> ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${LIBGCC} ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingwex.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libucrtbase_static.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libmoxnox.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libucrtbase.a")

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(EXCLUDE_LIBS "-Wl,--exclude-libs=ALL")
else()
    set(EXCLUDE_LIBS "")
endif()

set(CMAKE_CXX_CREATE_SHARED_LIBRARY "${CMAKE_CXX_CREATE_SHARED_LIBRARY} ${EXCLUDE_LIBS} ${LIBSTDCXX} ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libucrtbase_static.a ${LIBEH} ${LIBGCC} ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingwex.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libmoxnox.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libucrtbase.a")
set(CMAKE_C_CREATE_SHARED_LIBRARY "${CMAKE_C_CREATE_SHARED_LIBRARY} ${EXCLUDE_LIBS} ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libucrtbase_static.a ${LIBGCC} ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingwex.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libmoxnox.a ${MOXNOX_BUILD_ROOT}/ports/mingw-w64/libmingw32.a ${MOXNOX_BUILD_ROOT}/libmoxnox/libucrtbase.a")
