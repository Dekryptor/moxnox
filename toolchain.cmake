if (USE_COMPILER_RT)
    set(LIBGCC "-lclang_rt.builtins")
else()
    set(LIBGCC "-lgcc")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(LIBSTDCXX "-lc++ -lc++abi")
    set(LIBEH "-lunwind")
else()
    if (USE_COMPILER_RT)
        set(LIBSTDCXX "-lc++ -lc++abi")
        set(LIBEH "-lunwind")
    else()
        set(LIBSTDCXX "-lstdc++")
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

set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> ${CMAKE_GNULD_IMAGE_VERSION} <LINK_LIBRARIES> ${LIBSTDCXX_EXE} ports/mingw-w64/libmingw32.a ${LIBEH} ${LIBGCC} ports/mingw-w64/libmingwex.a ports/mingw-w64/libucrtbase_static.a libmoxnox/libmoxnox.a ports/mingw-w64/libmingw32.a libmoxnox/libucrtbase.a")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> ${CMAKE_GNULD_IMAGE_VERSION} <LINK_LIBRARIES> ports/mingw-w64/libmingw32.a ${LIBGCC} ports/mingw-w64/libmingwex.a ports/mingw-w64/libucrtbase_static.a libmoxnox/libmoxnox.a ports/mingw-w64/libmingw32.a libmoxnox/libucrtbase.a")

set(CMAKE_CXX_CREATE_SHARED_LIBRARY "${CMAKE_CXX_CREATE_SHARED_LIBRARY} ${LIBSTDCXX} ports/mingw-w64/libmingw32.a ports/mingw-w64/libucrtbase_static.a ${LIBEH} ${LIBGCC} ports/mingw-w64/libmingwex.a libmoxnox/libmoxnox.a ports/mingw-w64/libmingw32.a libmoxnox/libucrtbase.a")
set(CMAKE_C_CREATE_SHARED_LIBRARY "${CMAKE_C_CREATE_SHARED_LIBRARY} ports/mingw-w64/libmingw32.a ports/mingw-w64/libucrtbase_static.a ${LIBGCC} ports/mingw-w64/libmingwex.a libmoxnox/libmoxnox.a ports/mingw-w64/libmingw32.a libmoxnox/libucrtbase.a")
