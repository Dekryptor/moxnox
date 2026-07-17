include(${CMAKE_SOURCE_DIR}/rust-port.cmake)

add_rust_port(ripgrep rg.exe PATCHED_CRATES memmap2)
