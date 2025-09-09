cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_C_STANDARD_LIBRARIES "")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

# ------------------

include_directories(../include) # FIXME

add_executable(kernel kernel.cpp)
target_compile_options(kernel PRIVATE -stdlib=libc++ -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Wno-unused-const-variable -Werror=pointer-arith -fno-exceptions -fno-rtti -fcheck-new -Wno-invalid-offsetof -fno-threadsafe-statics -fvisibility=hidden -fno-auto-import -mno-red-zone -mno-stack-arg-probe -gdwarf-5)
target_link_options(kernel PRIVATE -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000 -Wl,--file-alignment=0x1000)

set(CMAKE_C_LINK_EXECUTABLE "ld <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>") # HACK

# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/payload.s.in ${CMAKE_CURRENT_BINARY_DIR}/payload.s)

set(SRC_FILES boot.S
    boot.cpp
    mem.S
    payload.S)

add_executable(boot ${SRC_FILES})
add_dependencies(boot kernel)

target_compile_options(boot PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Wno-unused-const-variable -Werror=pointer-arith -fno-exceptions -fno-rtti -fcheck-new -Wno-invalid-offsetof -fno-threadsafe-statics -fvisibility=hidden -fno-auto-import -mno-red-zone -mno-stack-arg-probe -gdwarf-5 -fno-asynchronous-unwind-tables -mstrict-align)
target_compile_options(boot PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
target_link_options(boot PRIVATE -T ${CMAKE_SOURCE_DIR}/boot.ld)
set(CMAKE_EXE_LINKER_FLAGS "-static") # use binutils rather than llvm for linking

# Clang likes to generate .refptr stubs, stop it doing this
target_compile_options(boot PRIVATE $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-fno-auto-import>)

# set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/payload.s PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/kernel/kernel.exe)

set_property(TARGET boot PROPERTY LINKER_LANGUAGE C) # HACK
set_property(TARGET boot PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/boot.ld)
set_property(TARGET boot PROPERTY SUFFIX "")

install(TARGETS boot DESTINATION ${CMAKE_INSTALL_PREFIX})
