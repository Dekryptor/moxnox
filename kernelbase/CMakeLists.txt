cmake_minimum_required(VERSION 3.28)

project(kernelbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/kernelbase.cpp)

add_library(kernelbase SHARED ${SRC_FILES})

target_compile_options(kernelbase PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -gdwarf-5)
target_link_options(kernelbase PUBLIC -nostartfiles -nostdlib)

install(TARGETS kernelbase DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
