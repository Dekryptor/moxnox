cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/kernel.cpp
    src/mem.cpp
    src/console.cpp
    src/interrupts.cpp
    src/ints.S
    src/timer.S
    src/timer.cpp
    src/scheduler.cpp
    src/handle.cpp
    src/smp.cpp
    src/smp.S
    src/pe.cpp
    src/keyboard.cpp
    src/keyboard.S
    src/event.cpp
    src/pipe.cpp
    src/pci.cpp
    src/sata.cpp
    src/sata.S
    src/semaphore.cpp
    src/volume.cpp
    src/btrfs.cpp
    src/process.cpp
    src/section.cpp
    src/string.cpp
    src/syscalls.S
    src/syscalls.cpp
    src/serial.cpp
    src/serial.S
    src/mem.S)

add_definitions(-DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
add_subdirectory(freetype)
target_compile_options(freetype PRIVATE -mno-stack-arg-probe)

add_executable(kernel ${SRC_FILES})
include_directories(freetype/include)

target_compile_options(kernel PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe)
target_link_options(kernel PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000)

target_link_libraries(kernel freetype)
