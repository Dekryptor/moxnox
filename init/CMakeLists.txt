cmake_minimum_required(VERSION 3.15)

project(init)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/init.cpp)

add_executable(init ${SRC_FILES})

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

target_compile_options(init PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(init PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(init ntdll ntdllcrt)

install(TARGETS init DESTINATION ${CMAKE_INSTALL_PREFIX})
