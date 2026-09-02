project(file VERSION 5.48)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
file(MAKE_DIRECTORY ${GEN}/src)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/magic.h.in MAGIC_H)
string(REPLACE "X.YY" "548" MAGIC_H "${MAGIC_H}")
file(CONFIGURE OUTPUT ${GEN}/magic.h CONTENT "${MAGIC_H}" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/src/file.h
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/file.h ${GEN}/src/file.h
    COMMAND ${HOST_SED} -z -i [[s|#ifndef WIN32\n#include <sys/param.h>\n#endif|#ifndef WIN32\n#ifdef HAVE_SYS_PARAM_H\n#include <sys/param.h>\n#endif\n#endif|]] ${GEN}/src/file.h
    DEPENDS repo/src/file.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/host/src/file
    COMMAND sh -c "rm -rf '${GEN}/host-src' '${GEN}/host' && cp -a '${CMAKE_CURRENT_SOURCE_DIR}/repo' '${GEN}/host-src' && rm -f '${GEN}/host-src/.git' && cd '${GEN}/host-src' && autoreconf -fi > /dev/null 2>&1 && mkdir -p '${GEN}/host' && cd '${GEN}/host' && '${GEN}/host-src/configure' --disable-bzlib --disable-lzlib --disable-xzlib --disable-libseccomp --disable-zlib --disable-zstdlib --disable-shared > /dev/null 2>&1 && make > /dev/null 2>&1 || make"
    DEPENDS repo/src/file.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/magic.mgc
    COMMAND sh -c "cd '${GEN}' && '${GEN}/host/src/file' -C -m '${CMAKE_CURRENT_SOURCE_DIR}/repo/magic/Magdir' && mv Magdir.mgc magic.mgc"
    DEPENDS ${GEN}/host/src/file
    VERBATIM
)

add_library(libmagic SHARED
    ${GEN}/magic.mgc
    ${GEN}/src/file.h
    repo/src/asctime_r.c
    repo/src/ctime_r.c
    repo/src/dprintf.c
    repo/src/fmtcheck.c
    repo/src/strcasestr.c
    repo/src/strlcat.c
    repo/src/strlcpy.c
    repo/src/buffer.c
    repo/src/magic.c
    repo/src/apprentice.c
    repo/src/softmagic.c
    repo/src/ascmagic.c
    repo/src/encoding.c
    repo/src/compress.c
    repo/src/is_csv.c
    repo/src/is_json.c
    repo/src/is_simh.c
    repo/src/is_tar.c
    repo/src/readelf.c
    repo/src/print.c
    repo/src/fsmagic.c
    repo/src/funcs.c
    repo/src/apptype.c
    repo/src/der.c
    repo/src/cdf.c
    repo/src/cdf_time.c
    repo/src/readcdf.c
    repo/src/swap.c
)

set_target_properties(libmagic PROPERTIES OUTPUT_NAME "magic-1" PREFIX "lib")

set(FILE_CFLAGS
    ${USERSPACE_CFLAGS}
    -include ${GEN}/src/file.h
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-attributes
)

target_compile_definitions(libmagic PRIVATE HAVE_CONFIG_H MAGIC="/share/misc/magic.mgc")
target_include_directories(libmagic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GEN})
target_compile_options(libmagic PRIVATE ${FILE_CFLAGS})
target_link_libraries(libmagic PRIVATE zlib moxnox)

add_executable(file
    ${GEN}/src/file.h
    repo/src/file.c
)

target_compile_definitions(file PRIVATE HAVE_CONFIG_H MAGIC="/share/misc/magic.mgc")
target_include_directories(file PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GEN})
target_compile_options(file PRIVATE ${FILE_CFLAGS})
target_link_libraries(file PRIVATE libmagic moxnox)

install(TARGETS file DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS libmagic DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES ${GEN}/magic.mgc DESTINATION ${CMAKE_INSTALL_PREFIX}/share/misc)
