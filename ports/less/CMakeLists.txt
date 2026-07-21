set(GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${GEN}")

add_custom_command(
    OUTPUT "${GEN}/screen.c"
    COMMAND sh -c "cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/screen.c' '${GEN}/screen.c' && patch -p0 -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/tcgetwinsize.patch'"
    DEPENDS
        repo/screen.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tcgetwinsize.patch
    VERBATIM
)

add_executable(less
    repo/brac.c
    repo/ch.c
    repo/charset.c
    repo/cmdbuf.c
    repo/command.c
    repo/cvt.c
    repo/decode.c
    repo/edit.c
    repo/evar.c
    repo/filename.c
    repo/forwback.c
    repo/ifile.c
    repo/input.c
    repo/jump.c
    repo/line.c
    repo/linenum.c
    repo/lsystem.c
    repo/main.c
    repo/mark.c
    repo/optfunc.c
    repo/option.c
    repo/opttbl.c
    repo/os.c
    repo/output.c
    repo/pattern.c
    repo/position.c
    repo/prompt.c
    ${GEN}/screen.c
    repo/search.c
    repo/signal.c
    repo/tags.c
    repo/ttyin.c
    repo/version.c
    repo/xbuf.c
    repo/lesskey_parse.c
    help.c
)

target_include_directories(less PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    repo
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(less PRIVATE _CRTIMP=)

target_compile_options(less PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-sign-compare
    -Wno-type-limits
)

target_link_libraries(less PRIVATE ncurses moxnox)

install(TARGETS less DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
