cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

configure_file(src/font.S.in ${CMAKE_CURRENT_BINARY_DIR}/font.S)

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
    src/mem.S
    src/file.cpp
    src/thread.cpp
    src/rtc.cpp
    src/dwarf.cpp
    src/power.cpp
    src/page_cache.cpp
    src/rwsem.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/font.S)

add_executable(kernel ${SRC_FILES})
include_directories(../freetype/include)

target_compile_options(kernel PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics)
# target_compile_options(kernel PRIVATE -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/= -ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=)
target_link_options(kernel PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000)

target_link_libraries(kernel freetype)

target_compile_definitions(kernel PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")

set_source_files_properties(src/mem.S src/syscalls.S src/ints.S src/keyboard.S src/sata.S PROPERTIES COMPILE_OPTIONS "-g0")

install(TARGETS kernel DESTINATION ${CMAKE_INSTALL_PREFIX})
