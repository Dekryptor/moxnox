include(${CMAKE_SOURCE_DIR}/rust-port.cmake)

add_rust_port(hexyl hexyl.exe PATCHED_CRATES terminal_size)
