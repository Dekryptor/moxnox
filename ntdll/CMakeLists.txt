cmake_minimum_required(VERSION 3.28)

project(ntdll)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/ntdll.cpp
    src/ntdll.def
    src/string.cpp
    src/mem.cpp
    src/heap.cpp
    src/except.cpp)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    list(APPEND SRC_FILES
        src/syscalls-aarch64.S
        src/continue-aarch64.S)
else()
    list(APPEND SRC_FILES
        src/syscalls-x86.S
        src/mem.S
        src/continue-x86.S)
endif()

add_library(ntdll SHARED ${SRC_FILES})

target_sources(ntdll PUBLIC FILE_SET CXX_MODULES FILES
    src/print.cpp
)

target_compile_options(ntdll PRIVATE ${USERSPACE_CFLAGS})
target_link_options(ntdll PUBLIC -nostartfiles -nostdlib)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    target_link_libraries(ntdll format dwarf list) # unwind
else()
    target_link_libraries(ntdll format dwarf unwind list)
endif()

install(TARGETS ntdll DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
