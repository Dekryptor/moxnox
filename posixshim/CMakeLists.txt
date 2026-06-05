cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_library(tre STATIC
    ../ports/tre/repo/lib/regcomp.c
    ../ports/tre/repo/lib/regexec.c
    ../ports/tre/repo/lib/regerror.c
    ../ports/tre/repo/lib/tre-ast.c
    ../ports/tre/repo/lib/tre-compile.c
    ../ports/tre/repo/lib/tre-match-backtrack.c
    ../ports/tre/repo/lib/tre-match-parallel.c
    ../ports/tre/repo/lib/tre-mem.c
    ../ports/tre/repo/lib/tre-parse.c
    ../ports/tre/repo/lib/tre-stack.c
)

target_include_directories(tre PRIVATE ../ports/tre)
target_compile_definitions(tre PRIVATE HAVE_CONFIG_H)
target_compile_options(tre PRIVATE -include ../ports/tre/tre-internal-fix.h)

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
    src/file.cpp
    src/wcwidth.cpp
    src/dir.cpp
)

target_compile_options(posixshim PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(posixshim PRIVATE print errno termios unicode unique_handle)
target_link_libraries(posixshim PRIVATE moxnox)
target_link_libraries(posixshim PRIVATE tre)
