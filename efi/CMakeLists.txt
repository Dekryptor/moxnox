cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_FILES src/efi.cpp
    src/mem.cpp)

add_executable(efi ${SRC_FILES})

set_target_properties(efi PROPERTIES SUFFIX ".efi")

include_directories(gnu-efi)
include_directories(gnu-efi/x86_64)

target_compile_options(efi PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(efi PUBLIC -nostartfiles -nostdlib -Wl,--subsystem,10 -eefi_main)
