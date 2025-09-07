cmake_minimum_required(VERSION 3.28)

project(ntdll)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/ntdll.cpp
    src/ntdll.def
    src/syscalls.S
    src/string.cpp
    src/mem.cpp
    src/heap.cpp
    src/mem.S
    src/except.cpp
    src/continue.S)

add_library(ntdll SHARED ${SRC_FILES})
target_sources(ntdll PUBLIC FILE_SET CXX_MODULES FILES
    src/print.cpp
)

target_compile_options(ntdll PRIVATE ${USERSPACE_CFLAGS})
target_link_options(ntdll PUBLIC -nostartfiles -nostdlib)
target_link_libraries(ntdll format_sse dwarf_sse unwind_sse list_sse)

install(TARGETS ntdll DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
