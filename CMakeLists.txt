cmake_minimum_required(VERSION 3.15)

project(moxnox)

add_subdirectory(kernel)
add_subdirectory(efi)
add_subdirectory(init)

add_definitions(-DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
add_subdirectory(freetype)
target_compile_options(freetype PRIVATE -mno-stack-arg-probe)
