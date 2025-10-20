cmake_minimum_required(VERSION 3.28)

project(ucrtbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_FILES src/ucrtbase.cpp)

add_library(ucrtbase SHARED ${SRC_FILES})

target_compile_options(ucrtbase PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ucrtbase PUBLIC -nostartfiles -nostdlib)

target_link_libraries(ucrtbase PRIVATE moxnox)

install(TARGETS ucrtbase DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
