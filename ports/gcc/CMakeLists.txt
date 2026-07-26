if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    # no aarch64-pc-moxnox GCC yet
    return()
endif()

set(GCC_HOST_TRIPLE x86_64-pc-moxnox)

execute_process(COMMAND gcc -dumpmachine
    OUTPUT_VARIABLE GCC_BUILD_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE)

cmake_host_system_information(RESULT GCC_BUILD_JOBS QUERY NUMBER_OF_LOGICAL_CORES)

set(GCC_STAGE ${CMAKE_CURRENT_BINARY_DIR}/stage)
set(GCC_DEST ${CMAKE_CURRENT_BINARY_DIR}/dest)

function(gcc_prereq name bootstrap_cmd configure_extra)
    set(src ${CMAKE_CURRENT_BINARY_DIR}/${name}-src)
    set(bld ${CMAKE_CURRENT_BINARY_DIR}/${name}-build)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.stamp
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${src} ${bld}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../${name}/repo ${src}
        COMMAND sh -c "cd '${src}' && ${bootstrap_cmd}"
        COMMAND ${CMAKE_COMMAND} -E copy /usr/share/gnuconfig/config.sub /usr/share/gnuconfig/config.guess ${src} # FIXME?
        COMMAND ${CMAKE_COMMAND} -E make_directory ${bld}
        COMMAND sh -c "export CONFIG_SITE=/usr/${GCC_HOST_TRIPLE}/etc/portage/config.site && cd '${bld}' && '${src}/configure' CC_FOR_BUILD=gcc --build=${GCC_BUILD_TRIPLE} --host=${GCC_HOST_TRIPLE} --prefix=/usr --disable-shared --enable-static ${configure_extra} && make -j${GCC_BUILD_JOBS} && make -j${GCC_BUILD_JOBS} install DESTDIR='${GCC_STAGE}'"
        # staged .la files record final /usr paths and poison later libtool links
        COMMAND sh -c "rm -f '${GCC_STAGE}/usr/lib/'*.la"
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${name}.stamp
        DEPENDS ${ARGN}
        COMMENT "Building GCC prerequisite ${name} (${GCC_HOST_TRIPLE})"
        VERBATIM
    )
endfunction()

gcc_prereq(gmp "./.bootstrap" "--disable-assembly 'CFLAGS=-O2 -std=gnu17'")
gcc_prereq(mpfr "./autogen.sh" "--with-gmp=${GCC_STAGE}/usr"
    ${CMAKE_CURRENT_BINARY_DIR}/gmp.stamp)
gcc_prereq(mpc "autoreconf -i" "--with-gmp=${GCC_STAGE}/usr --with-mpfr=${GCC_STAGE}/usr"
    ${CMAKE_CURRENT_BINARY_DIR}/mpfr.stamp)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libiconv.stamp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GCC_STAGE}/usr/include ${GCC_STAGE}/usr/lib
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/ports/libiconv/gen/include/iconv.h
        ${GCC_STAGE}/usr/include/iconv.h
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:iconv>
        ${GCC_STAGE}/usr/lib/libiconv.dll.a
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/libiconv.stamp
    DEPENDS iconv
    COMMENT "Staging libiconv for GCC (${GCC_HOST_TRIPLE})"
    VERBATIM
)

set(gcc_configure_args
    --build=${GCC_BUILD_TRIPLE}
    --host=${GCC_HOST_TRIPLE}
    --target=${GCC_HOST_TRIPLE}
    --prefix=/usr
    --with-pkgversion=Moxnox
    --enable-languages=c,c++
    --disable-nls
    --disable-werror
    --disable-multilib
    --enable-checking=release
    --enable-obsolete
    --enable-lto
    --enable-shared=libgcc
    --enable-threads=posix
    --disable-libstdcxx-pch
    --enable-libstdcxx-time
    --disable-libgomp
    --disable-libssp
    --disable-libada
    --disable-libvtv
    --disable-cet
    --disable-fixed-point
    --disable-fixincludes
    --disable-libunwind-exceptions
    --disable-systemtap
    --enable-targets=all
    --enable-default-pie
    --enable-host-pie
    --enable-default-ssp
    --without-isl
    --without-zstd
    --with-build-sysroot=/usr/${GCC_HOST_TRIPLE}
    --with-gmp=${GCC_STAGE}/usr
    --with-mpfr=${GCC_STAGE}/usr
    --with-mpc=${GCC_STAGE}/usr
    --with-libiconv-prefix=${GCC_STAGE}/usr
    --with-gxx-libcxx-include-dir=/include/c++/v1
)
list(JOIN gcc_configure_args " " gcc_configure_str)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gcc.stamp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/gcc-build
    COMMAND sh -c "export CONFIG_SITE=/usr/${GCC_HOST_TRIPLE}/etc/portage/config.site && cd '${CMAKE_CURRENT_BINARY_DIR}/gcc-build' && '${CMAKE_CURRENT_SOURCE_DIR}/repo/configure' ${gcc_configure_str} && make -j${GCC_BUILD_JOBS} && make -j${GCC_BUILD_JOBS} install-strip DESTDIR='${GCC_DEST}'"
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/gcc.stamp
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mpc.stamp ${CMAKE_CURRENT_BINARY_DIR}/libiconv.stamp
    USES_TERMINAL
    COMMENT "Building native GCC (${GCC_HOST_TRIPLE} hosted)"
    VERBATIM
)

add_custom_target(gcc
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gcc.stamp
)

install(DIRECTORY ${GCC_DEST}/usr/ DESTINATION ${CMAKE_INSTALL_PREFIX}/
    USE_SOURCE_PERMISSIONS OPTIONAL
)
