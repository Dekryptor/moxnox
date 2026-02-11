cmake_minimum_required(VERSION 3.28)

project(kernelbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/kernelbase.def
    src/kernelbase.cpp
    src/sync.cpp
    src/console.cpp
    src/stubs.S
    src/debug.cpp
)

add_library(kernelbase SHARED ${SRC_FILES})

target_compile_options(kernelbase PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(kernelbase PUBLIC -nostartfiles -nostdlib)

target_link_libraries(kernelbase PRIVATE format print)
target_link_libraries(kernelbase PRIVATE moxnox)

install(TARGETS kernelbase DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
