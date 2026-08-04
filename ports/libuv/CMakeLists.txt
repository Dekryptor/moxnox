set(LIBUV_BUILD_TESTS OFF)
set(LIBUV_BUILD_BENCH OFF)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE) # libuv is chatty
add_subdirectory(repo)

add_custom_target(libuv)
add_dependencies(libuv uv)

target_compile_options(uv PRIVATE ${USERSPACE_CFLAGS})

target_compile_options(uv PRIVATE
    -Wno-conversion
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-sign-compare
    -Wno-strict-prototypes
)
