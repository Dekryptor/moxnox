cmake_minimum_required(VERSION 3.28)

project(ntdll)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(SRC_FILES src/ntdll.cpp
        src/syscalls-aarch64.S
        src/string.cpp
        src/except.cpp
        src/continue-aarch64.S)
else()
    set(SRC_FILES src/ntdll.cpp
        src/ntdll.def
        src/syscalls-x86.S
        src/string.cpp
        src/mem.cpp
        src/heap.cpp
        src/mem.S
        src/except.cpp
        src/continue-x86.S)
endif()

add_library(ntdll SHARED ${SRC_FILES})

target_sources(ntdll PUBLIC FILE_SET CXX_MODULES FILES
    src/print.cpp
)

target_compile_options(ntdll PRIVATE ${USERSPACE_CFLAGS})
target_link_options(ntdll PUBLIC -nostartfiles -nostdlib)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    target_link_libraries(ntdll format dwarf) # unwind_sse list_sse)
else()
    target_link_libraries(ntdll format_sse dwarf_sse unwind_sse list_sse)
endif()

install(TARGETS ntdll DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
