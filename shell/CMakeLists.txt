cmake_minimum_required(VERSION 3.15)

project(shell)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/shell.cpp
    src/ls.cpp
    src/string.cpp)

add_executable(shell ${SRC_FILES})

target_compile_options(shell PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(shell PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(shell ntdll ntdllcrt)

install(TARGETS shell DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(echo src/echo.cpp
    src/string.cpp)

target_compile_options(echo PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(echo PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(echo ntdll ntdllcrt)

install(TARGETS echo DESTINATION ${CMAKE_INSTALL_PREFIX})
