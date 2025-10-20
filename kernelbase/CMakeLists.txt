cmake_minimum_required(VERSION 3.28)

project(kernelbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/kernelbase.cpp
    src/sync.cpp
    src/console.cpp
)

add_library(kernelbase SHARED ${SRC_FILES})

target_sources(kernelbase PUBLIC FILE_SET CXX_MODULES FILES
    src/print.cpp)

target_compile_options(kernelbase PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(kernelbase PUBLIC -nostartfiles -nostdlib)

target_link_libraries(kernelbase PRIVATE format)
target_link_libraries(kernelbase PRIVATE moxnox)

install(TARGETS kernelbase DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
