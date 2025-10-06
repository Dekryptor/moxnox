cmake_minimum_required(VERSION 3.28)

project(init)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/init.cpp)

add_executable(init ${SRC_FILES})
target_sources(init PUBLIC FILE_SET CXX_MODULES FILES
    src/print.cpp
)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

target_compile_options(init PRIVATE ${USERSPACE_CFLAGS})
target_link_options(init PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(init format cmdline)
target_link_libraries(init moxnox)

install(TARGETS init DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
