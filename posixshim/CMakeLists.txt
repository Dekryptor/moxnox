cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_library(posixshim STATIC src/stubs.S)

target_compile_options(posixshim PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(kernelbase PRIVATE moxnox)
