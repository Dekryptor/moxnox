project(flex VERSION 2.6.4)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

configure_file(config.h.in ${GEN}/config.h @ONLY)

add_custom_command(
    OUTPUT ${GEN}/src/skel.c
    COMMAND sh -c "sh '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mkskel.sh' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src' '${HOST_M4}' ${PROJECT_VERSION} > '${GEN}/src/skel.c'"
    DEPENDS
        repo/src/mkskel.sh
        repo/src/flex.skl
        repo/src/flexint.h
        repo/src/tables_shared.h
        repo/src/tables_shared.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/parse.c ${GEN}/src/parse.h
    COMMAND ${HOST_BISON} -y -d -o ${GEN}/src/parse.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/parse.y
    DEPENDS repo/src/parse.y
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/scan.c
    COMMAND ${HOST_FLEX} -o ${GEN}/src/scan.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/scan.l
    DEPENDS repo/src/scan.l
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/tables.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tables.c ${GEN}/src/tables.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include "tables.h"\n|#include "tables.h"\n\n#include <arpa/inet.h>\n|]]
            ${GEN}/src/tables.c
    DEPENDS repo/src/tables.c
    VERBATIM
)

add_custom_target(flex_parse_header DEPENDS ${GEN}/src/parse.h)

set(FLEX_CFLAGS
    ${USERSPACE_CFLAGS}
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-misleading-indentation
    -Wno-format-truncation
    -Wno-pointer-sign
    $<$<C_COMPILER_ID:Clang>:-Wno-macro-redefined>
)

add_executable(flex
    ${GEN}/src/parse.c
    ${GEN}/src/scan.c
    ${GEN}/src/skel.c
    ${GEN}/src/tables.c
    repo/src/buf.c
    repo/src/ccl.c
    repo/src/dfa.c
    repo/src/ecs.c
    repo/src/filter.c
    repo/src/gen.c
    repo/src/main.c
    repo/src/misc.c
    repo/src/nfa.c
    repo/src/options.c
    repo/src/regex.c
    repo/src/scanflags.c
    repo/src/scanopt.c
    repo/src/sym.c
    repo/src/tables_shared.c
    repo/src/tblcmp.c
    repo/src/yylex.c
)

target_compile_definitions(flex PRIVATE HAVE_CONFIG_H LOCALEDIR=\"/share/locale\")
target_include_directories(flex PRIVATE ${GEN} ${GEN}/src ${CMAKE_CURRENT_SOURCE_DIR}/repo/src)
target_compile_options(flex PRIVATE ${FLEX_CFLAGS})
target_link_libraries(flex PRIVATE moxnox)

add_dependencies(flex flex_parse_header)

install(TARGETS flex DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(CODE "file(CREATE_LINK flex.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/flex++.exe\" SYMBOLIC)")
install(CODE "file(CREATE_LINK flex.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lex.exe\" SYMBOLIC)")

add_library(libfl STATIC
    repo/src/libmain.c
    repo/src/libyywrap.c
)

set_target_properties(libfl PROPERTIES OUTPUT_NAME fl PREFIX lib)

target_compile_definitions(libfl PRIVATE HAVE_CONFIG_H)
target_include_directories(libfl PRIVATE ${GEN} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src)
target_compile_options(libfl PRIVATE ${FLEX_CFLAGS})

install(TARGETS libfl DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES repo/src/FlexLexer.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
