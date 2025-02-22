cmake_minimum_required(VERSION 3.29)

project(moxnox)

set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_C_STANDARD_LIBRARIES "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

# ------------------

set(CMAKE_C_LINK_EXECUTABLE "g++ <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>") # HACK

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/payload.s.in ${CMAKE_CURRENT_BINARY_DIR}/payload.s)

add_library(efistub SHARED efi.cpp)

target_include_directories(efistub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../gnu-efi/inc ${CMAKE_CURRENT_SOURCE_DIR}/../gnu-efi/inc/x86_64)
target_compile_options(efistub PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -fno-asynchronous-unwind-tables -fno-zero-initialized-in-bss)
target_link_options(efistub PUBLIC -nostartfiles -nostdlib -Wl,--image-base=0 -Wl,--exclude-all-symbols -e efi_main)

add_custom_command(
    TARGET efistub
    POST_BUILD
    COMMAND objcopy -S -K efi_main -O coff-i386 "${CMAKE_CURRENT_BINARY_DIR}/efistub.dll" "${CMAKE_CURRENT_BINARY_DIR}/efi.cpp.obj"
)

set(SRC_FILES boot.S
    pm.s
    rm-main.cpp
    pm-main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/payload.s)

add_executable(boot ${SRC_FILES})
add_dependencies(boot kernel efistub)

target_compile_options(boot PUBLIC -march=i386 -m32 -fno-ident -fno-pic -ffreestanding -fno-exceptions -masm=intel)
target_link_options(boot PUBLIC -Wl,-T ${CMAKE_SOURCE_DIR}/bootloader/boot.ld -nostartfiles -nostdlib ${CMAKE_CURRENT_BINARY_DIR}/efi.cpp.obj)

set_source_files_properties(rm-main.cpp PROPERTIES COMPILE_OPTIONS "-m16;-Wa,--32")
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/payload.s PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/kernel/kernel.exe)
set_source_files_properties(boot.S PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/efi.cpp.obj")

set_property(TARGET boot PROPERTY LINKER_LANGUAGE C) # HACK
set_property(TARGET boot PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/bootloader/boot.ld)
set_property(TARGET boot PROPERTY SUFFIX "")

install(TARGETS boot DESTINATION ${CMAKE_INSTALL_PREFIX})
