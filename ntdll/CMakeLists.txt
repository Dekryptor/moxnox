cmake_minimum_required(VERSION 3.28)

project(ntdll)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/ntdll.cpp
    src/ntdll.def
    src/heap.cpp
    src/except.cpp
    src/time.cpp
    src/stubs.S)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    list(APPEND SRC_FILES
        src/syscalls-aarch64.S
        src/continue-aarch64.S)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    list(APPEND SRC_FILES
        src/syscalls-x86.S
        src/continue-x86.S)
endif()

add_library(ntdll SHARED ${SRC_FILES})

target_sources(ntdll PUBLIC FILE_SET CXX_MODULES FILES
    src/ntdll_private.cpp
)

target_compile_options(ntdll PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ntdll PUBLIC -nostartfiles -nostdlib)
target_link_libraries(ntdll PRIVATE format dwarf unwind list unicode nt ntdll_mod pe kernelbase_mod crt pagesize timepage unique_handle env print)

install(TARGETS ntdll DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
