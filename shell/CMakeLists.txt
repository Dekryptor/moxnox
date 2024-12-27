cmake_minimum_required(VERSION 3.15)

project(shell)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/shell.cpp
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

###

add_executable(ls src/ls.cpp
    src/string.cpp)

target_compile_options(ls PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(ls PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(ls ntdll ntdllcrt)

install(TARGETS ls DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(cat src/cat.cpp
    src/string.cpp)

target_compile_options(cat PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(cat PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(cat ntdll ntdllcrt)

install(TARGETS cat DESTINATION ${CMAKE_INSTALL_PREFIX})
