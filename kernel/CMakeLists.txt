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
    src/rwsem.cpp
    src/rangelock.cpp
    src/volume.cpp
    src/guid.cpp
    src/page_cache.cpp
    src/vfs.cpp
    src/inode.cpp
    src/cpio.cpp
    src/section.cpp
    src/image.cpp
    src/keyboard.cpp
    src/tmpfs.cpp
    src/process.cpp
    src/syscalls.cpp
    src/power.cpp
    src/smp.cpp
    src/kernel_images.cpp
    src/cpu_block.cpp
    src/spinlock.cpp
    src/pci.cpp
    src/sata.cpp
    src/btrfs.cpp
    src/crc32c.cpp
    src/nvme.cpp
    src/xhci.cpp
    src/usb.cpp
    src/usbhub.cpp
    src/usbhid.cpp
    src/virtio.cpp
    src/virtio_block.cpp
    src/virtio_console.cpp
)

set(AMD64_MODULES ${GLOBAL_MODULES}
    src/x86/inout.cpp
    src/x86/rtc.cpp
    src/x86/ps2keyboard.cpp
    src/x86/serial.cpp
    src/x86/apic.cpp
    src/framebuffer.cpp
)

set(AARCH64_MODULES ${GLOBAL_MODULES}
    src/aarch64/pl011.cpp
    src/aarch64/psci.cpp
)

set(SRC_FILES src/kernel.cpp
    src/handle.cpp
    src/file.cpp
    src/bsod2.cpp)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(SRC_ASM_FILES src/aarch64/ints.S
        src/aarch64/timer.S
        src/aarch64/smp.S)

    add_executable(kernel ${SRC_FILES} ${SRC_ASM_FILES})
    target_sources(kernel PUBLIC FILE_SET CXX_MODULES FILES ${AARCH64_MODULES})
    target_link_libraries(kernel format dwarf list unicode rbtree vt100 cp437 evdev unwind nt ntdll_mod)
else()
    set(SRC_ASM_FILES src/x86/ints.S
        src/x86/timer.S
        src/x86/syscalls.S
        src/x86/mem.S
        src/x86/smp.S
        src/x86/tlb.S
        ${CMAKE_CURRENT_BINARY_DIR}/font.S)

    add_executable(kernel ${SRC_FILES} ${SRC_ASM_FILES})
    target_sources(kernel PUBLIC FILE_SET CXX_MODULES FILES ${AMD64_MODULES})
    target_link_libraries(kernel format_kernel dwarf_kernel list_kernel unicode_kernel rbtree_kernel vt100_kernel cp437_kernel evdev_kernel unwind_kernel nt_kernel ntdll_kernel pe_kernel)
endif()

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
target_compile_definitions(kernel PUBLIC -D_LIBCPP_VERBOSE_ABORT=)

install(TARGETS kernel DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
