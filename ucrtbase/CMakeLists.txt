cmake_minimum_required(VERSION 3.28)

project(ucrtbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_FILES src/ucrtbase.cpp
    src/stubs.S
    src/ucrtbase.def
    src/errno.cpp
    src/data.cpp
    src/heap.cpp
    src/exit.cpp
)

add_library(ucrtbase SHARED ${SRC_FILES})
target_sources(ucrtbase PUBLIC FILE_SET CXX_MODULES FILES src/file.cpp)

target_compile_options(ucrtbase PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ucrtbase PUBLIC -nostartfiles -nostdlib)

target_link_libraries(ucrtbase PRIVATE format print)
target_link_libraries(ucrtbase PRIVATE moxnox)

install(TARGETS ucrtbase DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
