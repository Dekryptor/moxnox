cmake_minimum_required(VERSION 3.28)

project(kernel32)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/kernel32.def
    src/kernel32.cpp
)

add_library(kernel32 SHARED ${SRC_FILES})

target_compile_options(kernel32 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(kernel32 PUBLIC -nostartfiles -nostdlib)

target_link_libraries(kernel32 PRIVATE moxnox)

install(TARGETS kernel32 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
