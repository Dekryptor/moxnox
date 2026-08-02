set(CMAKE_SYSTEM_NAME Moxnox)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSROOT "/usr/aarch64-pc-moxnox")

set(CMAKE_C_COMPILER aarch64-pc-moxnox-clang)
set(CMAKE_CXX_COMPILER aarch64-pc-moxnox-clang++)
set(CMAKE_ASM_COMPILER aarch64-pc-moxnox-clang)
set(DLLTOOL llvm-dlltool)
set(AR llvm-ar)

set(CLANG_TARGET_TRIPLE aarch64-pc-moxnox)

set(CMAKE_C_FLAGS_INIT "-isystem ${CMAKE_CURRENT_LIST_DIR}/include")
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++ -nostdinc++ -isystem /usr/aarch64-pc-moxnox/usr/include/c++/v1 -isystem ${CMAKE_CURRENT_LIST_DIR}/include")

set(USE_COMPILER_RT ON CACHE BOOL "" FORCE)

if (STATIC_LIBCXX)
    add_link_options($<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-static-libstdc++>)
endif()

set(CMAKE_INCDIRS_BEFORE_SYSROOT ON)
set(LIBCXX_HEADERS /usr/aarch64-pc-moxnox/usr/include/c++/v1)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-pc-moxnox)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

add_compile_options(-DMOXNOX)
