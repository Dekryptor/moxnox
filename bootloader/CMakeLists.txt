cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
enable_language(ASM_NASM)

set(SRC_FILES src/entry.asm
    src/main.c
    src/pe.c)

add_executable(bootloader ${SRC_FILES})

#message(FATAL_ERROR "${CMAKE_CXX_LINK_EXECUTABLE}")
set(CMAKE_C_LINK_EXECUTABLE "ld -m elf_i386 -nostdlib -T ${CMAKE_CURRENT_SOURCE_DIR}/src/linker.ld <OBJECTS> -o <TARGET>")

target_compile_options(bootloader PRIVATE $<$<COMPILE_LANGUAGE:C>:-m32 -ffreestanding -fno-builtin -mno-sse -Wall -Wextra -Werror=implicit-function-declaration>)
