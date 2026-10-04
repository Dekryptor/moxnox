project(make VERSION 4.4.1)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    set(MAKE_HOST_TRIPLE x86_64-pc-moxnox)
else()
    set(MAKE_HOST_TRIPLE aarch64-pc-moxnox)
endif()

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
file(MAKE_DIRECTORY ${GEN}/src)
file(MAKE_DIRECTORY ${GEN}/lib)

configure_file(config.h.in ${GEN}/config.h @ONLY)

add_custom_command(
    OUTPUT ${GEN}/src/main.c ${GEN}/src/commands.c ${GEN}/src/getopt.c
        ${GEN}/src/getopt1.c ${GEN}/src/getopt.h ${GEN}/lib/fnmatch.c
        ${GEN}/lib/fnmatch.h ${GEN}/lib/glob.h ${GEN}/lib/alloca.h ${GEN}/lib/findprog-in.c
    COMMAND sh -c "cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/main.c' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/commands.c' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/getopt.c' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/getopt1.c' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/getopt.h' '${GEN}/src/' && cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/gl/lib/fnmatch.c' '${GEN}/lib/' && cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/gl/lib/fnmatch.in.h' '${GEN}/lib/fnmatch.h' && cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/gl/lib/glob.in.h' '${GEN}/lib/glob.h' && sed 's|@HAVE_ALLOCA_H@|1|g' '${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/repo/lib/alloca.in.h' > '${GEN}/lib/alloca.h' && cp '${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/repo/lib/findprog-in.c' '${GEN}/lib/' && patch -p1 -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch'"
    DEPENDS
        repo/src/main.c
        repo/src/commands.c
        repo/src/getopt.c
        repo/src/getopt1.c
        repo/src/getopt.h
        repo/gl/lib/fnmatch.c
        repo/gl/lib/fnmatch.in.h
        repo/gl/lib/glob.in.h
        ../gnulib/repo/lib/alloca.in.h
        ../gnulib/repo/lib/findprog-in.c
        ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    VERBATIM
)

add_executable(make
    ../gnulib/repo/lib/concat-filename.c
    ${GEN}/lib/findprog-in.c
    ../gnulib/repo/lib/getloadavg.c
    ${GEN}/lib/fnmatch.c
    repo/gl/lib/glob.c
    repo/src/ar.c
    repo/src/arscan.c
    ${GEN}/src/commands.c
    repo/src/default.c
    repo/src/dir.c
    repo/src/expand.c
    repo/src/file.c
    repo/src/function.c
    ${GEN}/src/getopt.c
    ${GEN}/src/getopt1.c
    repo/src/guile.c
    repo/src/hash.c
    repo/src/implicit.c
    repo/src/job.c
    repo/src/load.c
    repo/src/loadapi.c
    ${GEN}/src/main.c
    repo/src/misc.c
    repo/src/output.c
    repo/src/posixos.c
    repo/src/read.c
    repo/src/remake.c
    repo/src/remote-stub.c
    repo/src/rule.c
    repo/src/shuffle.c
    repo/src/signame.c
    repo/src/strcache.c
    repo/src/variable.c
    repo/src/version.c
    repo/src/vpath.c
)

set_source_files_properties(
    ../gnulib/repo/lib/concat-filename.c
    ${GEN}/lib/findprog-in.c
    PROPERTIES COMPILE_OPTIONS "-D_GL_CONFIG_H_INCLUDED=1;-Dstreq(a,b)=(strcmp(a,b)==0)"
)

target_compile_definitions(make PRIVATE
    HAVE_CONFIG_H
    NO_ARCHIVES
    LIBDIR="/lib"
    LOCALEDIR="/share/locale"
)

target_include_directories(make PRIVATE
    ${GEN}
    ${GEN}/src
    ${GEN}/lib
    repo/src
    ../gnulib/repo/lib
)

target_compile_options(make PRIVATE
    ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    $<$<C_COMPILER_ID:Clang>:-Wno-string-compare>
)

target_link_libraries(make PRIVATE moxnox)

install(TARGETS make DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
