cmake_minimum_required(VERSION 3.28)

project(shell)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_library(shelllib STATIC)
target_sources(shelllib PUBLIC FILE_SET CXX_MODULES FILES
    src/string.cpp
    src/print.cpp
)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

target_compile_options(shelllib PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_libraries(shelllib format)

###

set(SRC_FILES src/shell.cpp)

add_executable(shell ${SRC_FILES})

target_compile_options(shell PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(shell PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(shell shelllib)
target_link_libraries(shell -lntdll ntdllcrt)
target_link_libraries(shell format)

install(TARGETS shell DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(echo src/echo.cpp)

target_compile_options(echo PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(echo PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(echo shelllib)
target_link_libraries(echo ntdll ntdllcrt)

install(TARGETS echo DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(ls src/ls.cpp)

target_compile_options(ls PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(ls PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(ls shelllib)
target_link_libraries(ls format)
target_link_libraries(ls -lntdll ntdllcrt)

install(TARGETS ls DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(cat src/cat.cpp)

target_compile_options(cat PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(cat PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(cat shelllib)
target_link_libraries(cat format)
target_link_libraries(cat -lntdll ntdllcrt)

install(TARGETS cat DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(pwd src/pwd.cpp)

target_compile_options(pwd PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(pwd PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(pwd shelllib)
target_link_libraries(pwd ntdll ntdllcrt)

install(TARGETS pwd DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(yes src/yes.cpp)

target_compile_options(yes PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(yes PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(yes shelllib)
target_link_libraries(yes ntdll ntdllcrt)

install(TARGETS yes DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(mkdir src/mkdir.cpp)

target_compile_options(mkdir PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(mkdir PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(mkdir shelllib)
target_link_libraries(mkdir format)
target_link_libraries(mkdir -lntdll ntdllcrt)

install(TARGETS mkdir DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(touch src/touch.cpp)

target_compile_options(touch PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse)
target_link_options(touch PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(touch shelllib)
target_link_libraries(touch format)
target_link_libraries(touch -lntdll ntdllcrt)

install(TARGETS touch DESTINATION ${CMAKE_INSTALL_PREFIX})
