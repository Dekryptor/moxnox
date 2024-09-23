cmake_minimum_required(VERSION 3.15)

project(moxnox)

set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

include_directories(include)

add_subdirectory(kernel)
add_subdirectory(kernel2)
add_subdirectory(efi)
add_subdirectory(init)
add_subdirectory(shell)
add_subdirectory(ntdll)

add_subdirectory(freetype)
target_compile_definitions(freetype PRIVATE -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
target_compile_options(freetype PRIVATE -mno-stack-arg-probe)
