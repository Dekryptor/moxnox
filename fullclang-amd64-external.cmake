set(CMAKE_SYSTEM_NAME Moxnox)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSROOT "/usr/x86_64-pc-moxnox")

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_RC_COMPILER x86_64-pc-moxnox-windres)
set(CMAKE_ASM_COMPILER x86_64-pc-moxnox-gcc)
set(DLLTOOL llvm-dlltool)
set(AR llvm-ar)

set(CLANG_TARGET_TRIPLE x86_64-pc-moxnox)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-moxnox)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-moxnox)
set(CMAKE_ASM_COMPILER_TARGET x86_64-pc-moxnox)

set(CMAKE_C_FLAGS_INIT "-isystem ${CMAKE_CURRENT_LIST_DIR}/include")
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++ -nostdinc++ -isystem /usr/x86_64-pc-moxnox/usr/include/c++/v1 -isystem ${CMAKE_CURRENT_LIST_DIR}/include")

set(USE_COMPILER_RT ON CACHE BOOL "" FORCE)

if (STATIC_LIBCXX)
    add_link_options($<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-static-libstdc++>)
endif()

set(CMAKE_INCDIRS_BEFORE_SYSROOT ON)
set(LIBCXX_HEADERS /usr/x86_64-pc-moxnox/usr/include/c++/v1)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-pc-moxnox)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
