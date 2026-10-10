include(${CMAKE_SOURCE_DIR}/rust-port.cmake)

init_rust_crate_submodules(
    ports/rust/errno
    ports/rust/getrandom
    ports/rust/getrandom-0.2
    ports/rust/jobserver
    ports/rust/libc
    ports/rust/libloading
    ports/rust/libloading-0.9
    ports/rust/nix
    ports/rust/rand
    ports/rust/rustix
    ports/rust/stacker
    ports/rust/termize
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(RUST_TARGET x86_64-pc-moxnox)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(RUST_TARGET aarch64-pc-moxnox)
else()
    message(FATAL_ERROR "no Rust target for ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(RUST_BUILD_TRIPLE x86_64-unknown-linux-gnu)
else()
    message(FATAL_ERROR "no Rust build triple for ${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

find_program(RUSTC_STAGE0 rustc HINTS /usr/bin REQUIRED)
find_program(CARGO_STAGE0 cargo HINTS /usr/bin REQUIRED)
find_program(RUST_HOST_LLVM_CONFIG llvm-config REQUIRED)
find_program(RUST_PYTHON python3 REQUIRED)

set(RUSTC_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)

configure_file(bootstrap.toml.in ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.toml @ONLY)

set(RUSTC_STAGE1 ${RUSTC_BUILD_DIR}/${RUST_TARGET}/stage1)
set(rustc_main ${RUSTC_BUILD_DIR}/${RUST_BUILD_TRIPLE}/stage1-rustc/${RUST_TARGET}/release/rustc-main.exe)

add_custom_command(
    OUTPUT ${rustc_main}
    COMMAND ${RUST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/x.py build
        --stage 1
        --config ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.toml
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${RUSTC_BUILD_DIR}/${RUST_BUILD_TRIPLE}/stage1/lib/rustlib/${RUST_TARGET}/lib
        ${RUSTC_STAGE1}/lib/rustlib/${RUST_TARGET}/lib
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.toml
    DEPFILE ${RUSTC_BUILD_DIR}/${RUST_BUILD_TRIPLE}/stage1-rustc/${RUST_TARGET}/release/rustc-main.d
    JOB_POOL cargo_pool
    COMMENT "Building rustc (${RUST_TARGET})"
    VERBATIM
)

add_custom_target(rust DEPENDS ${rustc_main})

install(PROGRAMS ${RUSTC_STAGE1}/bin/rustc.exe ${RUSTC_STAGE1}/bin/rustdoc.exe
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin OPTIONAL)

install(DIRECTORY ${RUSTC_STAGE1}/lib/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    USE_SOURCE_PERMISSIONS OPTIONAL
    PATTERN rustlib EXCLUDE
)
install(CODE "
    file(GLOB _rustlib_contents \"${RUSTC_STAGE1}/lib/rustlib/${RUST_TARGET}/lib/*\")
    if(_rustlib_contents)
        file(REMOVE_RECURSE \"${CMAKE_INSTALL_PREFIX}/lib/rustlib/${RUST_TARGET}\")
    endif()")
install(DIRECTORY ${RUSTC_STAGE1}/lib/rustlib/${RUST_TARGET}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/rustlib
    USE_SOURCE_PERMISSIONS OPTIONAL
)
