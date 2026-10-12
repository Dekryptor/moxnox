include(${CMAKE_SOURCE_DIR}/rust-port.cmake)

rust_export_crate(memmap2 ${CMAKE_CURRENT_SOURCE_DIR}/memmap2 ${CMAKE_CURRENT_SOURCE_DIR}/memmap2.patch)

add_rust_port(ripgrep rg.exe PATCH moxnox.patch PATCHED_CRATES memmap2)
