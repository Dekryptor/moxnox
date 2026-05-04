cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_library(posixshim STATIC
    src/stubs.S
    src/termios.cpp
    src/sleep.cpp
    src/signal.cpp
)

target_compile_options(posixshim PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(posixshim PRIVATE print errno termios)
target_link_libraries(posixshim PRIVATE moxnox)
