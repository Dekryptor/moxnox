cmake_minimum_required(VERSION 3.28)

project(init)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_executable(init src/init.cpp)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

target_compile_options(init PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(init PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(init format cmdline pagesize env unique_handle print)
target_link_libraries(init moxnox)

install(TARGETS init DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
