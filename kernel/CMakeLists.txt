cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

configure_file(src/font.S.in ${CMAKE_CURRENT_BINARY_DIR}/font.S)

set(SRC_FILES src/kernel.cpp
    src/console.cpp
    src/ints.S
    src/timer.S
    src/timer.cpp
    src/scheduler.cpp
    src/handle.cpp
    src/smp.cpp
    src/smp.S
    src/event.cpp
    src/pipe.cpp
    src/semaphore.cpp
    src/volume.cpp
    src/process.cpp
    src/section.cpp
    src/syscalls.S
    src/syscalls.cpp
    src/mem.S
    src/file.cpp
    src/thread.cpp
    src/power.cpp
    src/page_cache.cpp
    src/rwsem.cpp
    src/bsod2.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/font.S)

add_executable(kernel ${SRC_FILES})
target_sources(kernel PUBLIC FILE_SET CXX_MODULES FILES
    src/inout.cpp
    src/print.cpp
    src/bsod.cpp
    src/tmpfs.cpp
    src/cpio.cpp
    src/acpi.cpp
    src/rtc.cpp
    src/keyboard.cpp
    src/null.cpp
    src/mem.cpp
    src/config.cpp
    src/list.cpp
    src/interrupts.cpp
    src/serial.cpp
    src/btrfs.cpp
    src/sata.cpp
    src/pci.cpp
    src/vfs.cpp
    src/string.cpp
    src/pe.cpp
    src/rbtree.cpp
    src/objects.cpp
)

target_link_libraries(kernel format dwarf unwind)

include_directories(../freetype/include)

target_compile_options(kernel PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5)
# target_compile_options(kernel PRIVATE -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/= -ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=)
target_link_options(kernel PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000 -Wl,--allow-multiple-definition)

target_compile_options(kernel PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fmodules-ts>)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-g0>)
target_compile_options(kernel PRIVATE $<$<CONFIG:DEBUG>:-fno-inline -fno-omit-frame-pointer>)

target_include_directories(kernel PRIVATE ../acpica/source/include)

target_link_libraries(kernel freetype)
target_link_libraries(kernel acpica)

target_compile_definitions(kernel PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")
target_compile_definitions(kernel PUBLIC -D_CRTIMP=)

install(TARGETS kernel DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
