include(${CMAKE_SOURCE_DIR}/rust-port.cmake)

rust_export_crate(terminal_size ${CMAKE_CURRENT_SOURCE_DIR}/terminal_size ${CMAKE_CURRENT_SOURCE_DIR}/terminal_size.patch)

add_rust_port(hexyl hexyl.exe PATCH moxnox.patch PATCHED_CRATES terminal_size)
