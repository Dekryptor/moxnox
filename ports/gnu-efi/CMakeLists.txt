add_library(gnu-efi STATIC)
target_sources(gnu-efi PUBLIC FILE_SET CXX_MODULES FILES gnu-efi.cpp)
target_include_directories(gnu-efi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/repo/inc)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    target_include_directories(gnu-efi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/repo/inc/x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_include_directories(gnu-efi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/repo/inc/aarch64)
endif()

target_compile_options(gnu-efi PRIVATE ${KERNEL_CFLAGS})
