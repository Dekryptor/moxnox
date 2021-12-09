cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_FILES src/kernel.cpp)

add_executable(kernel ${SRC_FILES})

target_compile_options(kernel PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti)
target_link_options(kernel PUBLIC -nostartfiles -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000)
