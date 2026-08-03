set(BUILD_SHARED_LIBS ON)
set(XZ_NLS OFF)

# scripts need a proper shell
set(XZ_TOOL_SCRIPTS OFF)

add_subdirectory(repo)

target_include_directories(liblzma INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/repo/src/liblzma/api>
)

target_compile_options(liblzma PRIVATE ${USERSPACE_CFLAGS} -Wno-conversion -Wno-attributes)
target_compile_options(xzdec PRIVATE ${USERSPACE_CFLAGS} -Wno-conversion)
target_compile_options(lzmadec PRIVATE ${USERSPACE_CFLAGS})
target_compile_options(lzmainfo PRIVATE ${USERSPACE_CFLAGS})
target_compile_options(xz PRIVATE ${USERSPACE_CFLAGS} -Wno-conversion)
