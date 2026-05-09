cmake_minimum_required(VERSION 3.28)

project(iphlpapi)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/iphlpapi.cpp
    src/stubs.S
)

add_library(iphlpapi SHARED ${SRC_FILES})

target_compile_options(iphlpapi PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(iphlpapi PRIVATE -nostartfiles -nostdlib)

target_link_libraries(iphlpapi PRIVATE moxnox)

install(TARGETS iphlpapi
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
