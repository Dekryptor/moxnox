cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

configure_file(src/font.S.in ${CMAKE_CURRENT_BINARY_DIR}/font.S)

set(GLOBAL_MODULES src/cmdline.cpp
    src/string.cpp
    src/mem.cpp
    src/config.cpp
    src/thread.cpp
    src/object_ref.cpp
    src/objects.cpp
    src/visit.cpp
    src/print.cpp
    src/console.cpp
    src/bsod.cpp
    src/acpi.cpp
    src/module_init.cpp
    src/pipe.cpp
    src/event.cpp
    src/chardev.cpp
    src/interrupts.cpp
    src/timer.cpp
    src/disable_interrupts.cpp
    src/scheduler.cpp
    src/null.cpp
    src/semaphore.cpp
)

set(AMD64_MODULES ${GLOBAL_MODULES}
    src/inout.cpp
    src/tmpfs.cpp
    src/cpio.cpp
    src/rtc.cpp
    src/ps2keyboard.cpp
    src/serial.cpp
    src/btrfs.cpp
    src/sata.cpp
    src/pci.cpp
    src/vfs.cpp
    src/pe.cpp
    src/syscalls.cpp
    src/smp.cpp
    src/volume.cpp
    src/rwsem.cpp
    src/nvme.cpp
    src/virtio.cpp
    src/virtio_block.cpp
    src/virtio_console.cpp
    src/xhci.cpp
    src/usbhid.cpp
    src/usb.cpp
    src/usbhub.cpp
    src/keyboard.cpp
    src/rangelock.cpp
    src/page_cache.cpp
    src/framebuffer.cpp
    src/section.cpp
    src/process.cpp
    src/image.cpp
    src/guid.cpp
    src/power.cpp
    src/apic.cpp
    src/inode.cpp
)

set(AARCH64_MODULES ${GLOBAL_MODULES}
    src/pl011.cpp
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(SRC_FILES src/kernel.cpp
        src/handle.cpp
        src/bsod2.cpp)

    set(SRC_ASM_FILES src/ints-aarch64.S
        src/timer-aarch64.S
        src/mem-aarch64.S)

    add_executable(kernel ${SRC_FILES} ${SRC_ASM_FILES})
    target_sources(kernel PUBLIC FILE_SET CXX_MODULES FILES ${AARCH64_MODULES})
else()
    set(SRC_FILES src/kernel.cpp
        src/handle.cpp
        src/file.cpp
        src/bsod2.cpp)

    set(SRC_ASM_FILES src/ints-x86.S
        src/timer-x86.S
        src/smp.S
        src/syscalls.S
        src/mem-x86.S
        src/tlb.S
        ${CMAKE_CURRENT_BINARY_DIR}/font.S)

    add_executable(kernel ${SRC_FILES} ${SRC_ASM_FILES})
    target_sources(kernel PUBLIC FILE_SET CXX_MODULES FILES ${AMD64_MODULES})
    target_link_libraries(kernel unwind)
endif()

target_link_libraries(kernel format dwarf list unicode rbtree vt100 cp437 evdev)

target_compile_options(kernel PRIVATE ${KERNEL_CFLAGS} -Wno-unused-const-variable)
# target_compile_options(kernel PRIVATE -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/= -ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=)
target_link_options(kernel PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000 -Wl,--file-alignment=0x1000)

target_compile_options(kernel PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fmodules-ts>)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-g0>)
target_compile_options(kernel PRIVATE $<$<CONFIG:DEBUG>:-fno-inline -fno-omit-frame-pointer>)
target_compile_options(kernel PRIVATE $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-fno-auto-import>)

target_include_directories(kernel PRIVATE ../acpica/source/include)
target_link_libraries(kernel acpica)

if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    target_link_libraries(kernel freetype-static)
endif()

target_compile_definitions(kernel PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")
target_compile_definitions(kernel PUBLIC -D_CRTIMP=)

install(TARGETS kernel DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
