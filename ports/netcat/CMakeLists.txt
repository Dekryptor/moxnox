set(GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/repo")
set(PATCHES "${REPO}/debian/patches")
set(OPENBSD_COMPAT "${CMAKE_SOURCE_DIR}/ports/openssh/repo/openbsd-compat")

add_custom_command(
    OUTPUT ${GEN}/netcat.c ${GEN}/socks.c
    COMMAND sh -c "mkdir -p '${GEN}' && cp '${REPO}/netcat.c' '${REPO}/socks.c' '${REPO}/nc.1' '${REPO}/Makefile' '${GEN}/' && for p in port-to-linux-with-libbsd build-without-TLS-support connect-timeout send-crlf quit-timer udp-scan-timeout serialized-handling-multiple-clients make-getnameinfo-errors-nonfatal-in-report_sock; do patch -p1 -s -d '${GEN}' < \"${PATCHES}/$p.patch\" || exit 1; done && patch -p1 -s -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch'"
    DEPENDS
        repo/netcat.c
        repo/socks.c
        repo/nc.1
        repo/Makefile
        ${PATCHES}/port-to-linux-with-libbsd.patch
        ${PATCHES}/build-without-TLS-support.patch
        ${PATCHES}/connect-timeout.patch
        ${PATCHES}/send-crlf.patch
        ${PATCHES}/quit-timer.patch
        ${PATCHES}/udp-scan-timeout.patch
        ${PATCHES}/serialized-handling-multiple-clients.patch
        ${PATCHES}/make-getnameinfo-errors-nonfatal-in-report_sock.patch
        ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    VERBATIM
)

add_executable(nc
    ${GEN}/netcat.c
    ${GEN}/socks.c
    repo/atomicio.c
    ${OPENBSD_COMPAT}/arc4random.c
    ${OPENBSD_COMPAT}/arc4random_uniform.c
    ${OPENBSD_COMPAT}/base64.c
    ${OPENBSD_COMPAT}/bsd-misc.c
    ${OPENBSD_COMPAT}/explicit_bzero.c
    ${OPENBSD_COMPAT}/readpassphrase.c
    ${OPENBSD_COMPAT}/strtonum.c
    compat.c
)

target_include_directories(nc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat-include
    ${OPENBSD_COMPAT}
    repo
)

target_compile_options(nc PRIVATE
    ${USERSPACE_CFLAGS}
    -std=gnu17
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-unterminated-string-initialization
)

target_link_libraries(nc PRIVATE moxnox)

install(TARGETS nc DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_custom_target(netcat)
add_dependencies(netcat
    nc
)
