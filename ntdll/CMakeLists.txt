cmake_minimum_required(VERSION 3.15)

project(ntdll)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/ntdll.cpp
    src/syscalls.S
    src/string.cpp
    src/mem.cpp
    src/heap.cpp)

add_library(ntdll SHARED ${SRC_FILES})

target_compile_options(ntdll PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(ntdll PUBLIC -nostartfiles -nostdlib)
