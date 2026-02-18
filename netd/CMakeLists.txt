cmake_minimum_required(VERSION 3.28)

project(netd)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

###

add_executable(netd src/netd.cpp)

target_compile_options(netd PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(netd moxnox print ipv6)

install(TARGETS netd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
