cmake_minimum_required(VERSION 3.29)

project(moxnox)

set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_C_STANDARD_LIBRARIES "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_executable(payload payload.cpp)

target_link_options(payload PUBLIC -nostartfiles -nostdlib -Wl,--entry=start -Wl,--image-base=0xffff800000000000)

# ------------------

set(CMAKE_C_LINK_EXECUTABLE "g++ <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>") # HACK

set(SRC_FILES boot.s
    pm.s
    rm-main.cpp
    pm-main.cpp
    payload.s)

add_executable(boot ${SRC_FILES})
add_dependencies(boot payload)

target_compile_options(boot PUBLIC -march=i386 -m32 -fno-ident -fno-pic -ffreestanding -fno-leading-underscore -fno-exceptions -masm=intel)
target_link_options(boot PUBLIC -Wl,-T ${CMAKE_SOURCE_DIR}/kernel2/boot.ld -nostartfiles -nostdlib)

set_source_files_properties(rm-main.cpp PROPERTIES COMPILE_OPTIONS "-m16;-Wa,--32")
set_source_files_properties(payload.s PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/kernel2/payload.exe)

set_property(TARGET boot PROPERTY LINKER_LANGUAGE C) # HACK
set_property(TARGET boot PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/kernel2/boot.ld)
set_property(TARGET boot PROPERTY SUFFIX "")
