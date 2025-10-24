cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_C_STANDARD_LIBRARIES "")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

# ------------------

set(CMAKE_C_LINK_EXECUTABLE "g++ <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>") # HACK

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/payload.s.in ${CMAKE_CURRENT_BINARY_DIR}/payload.s)

add_library(efistub SHARED ../efi.cpp)

target_include_directories(efistub PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../gnu-efi/inc ${CMAKE_CURRENT_SOURCE_DIR}/../../gnu-efi/inc/x86_64)
target_compile_options(efistub PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -mno-sse -fno-threadsafe-statics -fvisibility=hidden -fno-asynchronous-unwind-tables -fno-zero-initialized-in-bss)
target_link_options(efistub PUBLIC -nostartfiles -nostdlib -Wl,--image-base=0 -Wl,--exclude-all-symbols -e efi_main)
target_link_libraries(efistub format_kernel pe_kernel)

add_custom_command(
    TARGET efistub
    POST_BUILD
    COMMAND objcopy -S -K efi_main -O pe-i386 --section-alignment 0x1000 "${CMAKE_CURRENT_BINARY_DIR}/efistub.dll" "${CMAKE_CURRENT_BINARY_DIR}/efi.cpp.obj"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/efi.cpp.obj"
)

set(SRC_FILES boot.S
    pm.s
    pm-main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/payload.s)

add_executable(boot ${SRC_FILES})
add_dependencies(boot kernel efistub)

target_compile_options(boot PUBLIC -march=i386 -m32 -fno-ident -fno-pic -ffreestanding -fno-exceptions -fno-rtti -masm=intel)
target_link_options(boot PUBLIC -Wl,-T ${CMAKE_SOURCE_DIR}/bootloader/x86/boot.ld -nostartfiles -nostdlib ${CMAKE_CURRENT_BINARY_DIR}/efi.cpp.obj)
target_link_libraries(boot pe_32bit)
set(CMAKE_EXE_LINKER_FLAGS "-static") # use binutils rather than llvm for linking

# Clang likes to generate .refptr stubs, stop it doing this
target_compile_options(boot PUBLIC $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-fno-auto-import>)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/payload.s PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/kernel/kernel.exe)
set_source_files_properties(boot.S PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/efi.cpp.obj")

set_property(TARGET boot PROPERTY LINKER_LANGUAGE C) # HACK
set_property(TARGET boot PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/bootloader/x86/boot.ld)
set_property(TARGET boot PROPERTY SUFFIX "")

install(TARGETS boot DESTINATION ${CMAKE_INSTALL_PREFIX})
