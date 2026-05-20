cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_library(posixshim STATIC
    src/stubs.S
    src/termios.cpp
    src/sleep.cpp
    src/signal.cpp
    src/dirname.cpp
    src/time.cpp
    src/security.cpp
    src/poll.cpp
    src/environ.cpp
)

target_compile_options(posixshim PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_include_directories(posixshim PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(posixshim PRIVATE print errno termios)
target_link_libraries(posixshim PRIVATE moxnox)
