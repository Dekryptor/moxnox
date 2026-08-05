set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

file(MAKE_DIRECTORY ${GEN}/src)

add_custom_command(
    OUTPUT ${GEN}/src/line_printer.cc
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/line_printer.cc ${GEN}/src/line_printer.cc
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include <sys/ioctl.h>\n||]]
            -e [[s|    if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) && size.ws_col) {|    if (tcgetwinsize(STDOUT_FILENO, \&size) == 0 \&\& size.ws_col) {|]]
            ${GEN}/src/line_printer.cc
    DEPENDS repo/src/line_printer.cc
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/util.cc
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util.cc ${GEN}/src/util.cc
    COMMAND ${HOST_SED} -z -i
            -e [[s|#else\ndouble GetLoadAverage() {\n  double loadavg\[3\]|#elif defined(__moxnox__)\ndouble GetLoadAverage() {\n    return -0.0f\;\n}\n#else\ndouble GetLoadAverage() {\n  double loadavg[3]|]]
            ${GEN}/src/util.cc
    DEPENDS repo/src/util.cc
    VERBATIM
)

add_custom_target(ninja_patched DEPENDS ${GEN}/src/line_printer.cc ${GEN}/src/util.cc)

set(BUILD_TESTING OFF)

add_subdirectory(repo)

get_target_property(NINJA_SOURCES libninja SOURCES)
list(TRANSFORM NINJA_SOURCES REPLACE "^src/line_printer\\.cc$" "${GEN}/src/line_printer.cc")
list(TRANSFORM NINJA_SOURCES REPLACE "^src/util\\.cc$" "${GEN}/src/util.cc")
set_target_properties(libninja PROPERTIES SOURCES "${NINJA_SOURCES}")
set_source_files_properties(${GEN}/src/line_printer.cc ${GEN}/src/util.cc
    TARGET_DIRECTORY libninja PROPERTIES GENERATED TRUE)
target_include_directories(libninja PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/repo/src)
add_dependencies(libninja ninja_patched)

set(NINJA_CFLAGS ${USERSPACE_CFLAGS}
    -Wno-conversion
    -Wno-unused-parameter
)

target_compile_options(libninja PRIVATE ${NINJA_CFLAGS})
target_compile_options(libninja-re2c PRIVATE ${NINJA_CFLAGS})
target_compile_options(ninja PRIVATE ${NINJA_CFLAGS})
