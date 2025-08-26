cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(moxnox STATIC IMPORTED GLOBAL)
# set_target_properties(moxnox PROPERTIES LINKER_LANGUAGE CXX)

add_custom_command(
    OUTPUT libmoxnox.a
    COMMAND ${DLLTOOL} -d ${CMAKE_CURRENT_SOURCE_DIR}/ntdll.def -l ${CMAKE_CURRENT_BINARY_DIR}/libmoxnox.a
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ntdll.def
)
add_custom_target(moxnox_target ALL DEPENDS libmoxnox.a)

set_source_files_properties(libmoxnox.a PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ntdll.def)

add_dependencies(moxnox moxnox_target)
set_target_properties(moxnox PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/libmoxnox.a)
