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

target_compile_options(ntdll PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(ntdll PUBLIC -nostartfiles -nostdlib -Wl,--allow-multiple-definition)
target_link_libraries(ntdll format dwarf)

install(TARGETS ntdll DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
