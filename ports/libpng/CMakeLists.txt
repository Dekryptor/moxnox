set(PNG_SHARED ON)
set(PNG_STATIC OFF)
set(PNG_TESTS OFF)
set(PNG_TOOLS OFF)
set(SKIP_INSTALL_ALL ON)

set(ld-version-script OFF)

set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libzlib.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zlib/repo)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
add_subdirectory(repo)

set_target_properties(png_shared PROPERTIES DEBUG_POSTFIX "")

target_compile_options(png_shared PRIVATE
    ${USERSPACE_CFLAGS}
    -fvisibility=default
    -Wno-sign-conversion
    -Wno-conversion
)

get_target_property(libs png_shared LINK_LIBRARIES)
list(REMOVE_ITEM libs m)
set_target_properties(png_shared PROPERTIES LINK_LIBRARIES "${libs}")

get_target_property(libs png_shared INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM libs m)
set_target_properties(png_shared PROPERTIES INTERFACE_LINK_LIBRARIES "${libs}")

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies png_shared zlib)

install(TARGETS png_shared DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)

add_custom_target(libpng)
add_dependencies(libpng
    png_shared
)
