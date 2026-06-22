cmake_minimum_required(VERSION 3.28)

project(shell)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

###

set(SRC_FILES src/shell.cpp)

add_executable(shell ${SRC_FILES})
target_sources(shell PUBLIC FILE_SET CXX_MODULES FILES
    src/cd.cpp
)

target_compile_options(shell PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    target_compile_options(shell PRIVATE -masm=intel)
endif()

target_link_libraries(shell print unicode unique_handle list pagesize termios posixshim)
target_link_libraries(shell format)
target_link_libraries(shell readline)

install(TARGETS shell DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(echo src/echo.cpp)

target_compile_options(echo PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(echo print unicode format)
target_link_libraries(echo moxnox)

install(TARGETS echo DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ls src/ls.cpp)

target_compile_options(ls PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(ls print)
target_link_libraries(ls format list unicode errno)
target_link_libraries(ls moxnox posixshim)

install(TARGETS ls DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(cat src/cat.cpp)

target_compile_options(cat PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(cat print)
target_link_libraries(cat format unicode)
target_link_libraries(cat moxnox)

install(TARGETS cat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(pwd src/pwd.cpp)

target_compile_options(pwd PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(pwd print unicode format)
target_link_libraries(pwd moxnox)

install(TARGETS pwd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(yes src/yes.cpp)

target_compile_options(yes PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(yes print format)
target_link_libraries(yes moxnox)

install(TARGETS yes DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mkdir src/mkdir.cpp)

target_compile_options(mkdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mkdir print)
target_link_libraries(mkdir format unicode)
target_link_libraries(mkdir moxnox)

install(TARGETS mkdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(touch src/touch.cpp)

target_compile_options(touch PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(touch print)
target_link_libraries(touch format unicode)
target_link_libraries(touch moxnox)

install(TARGETS touch DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(rm src/rm.cpp)

target_compile_options(rm PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(rm print)
target_link_libraries(rm format unicode)
target_link_libraries(rm moxnox)

install(TARGETS rm DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(rmdir src/rmdir.cpp)

target_compile_options(rmdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(rmdir print)
target_link_libraries(rmdir format unicode)
target_link_libraries(rmdir moxnox)

install(TARGETS rmdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(screenshot src/screenshot.cpp)

target_compile_options(screenshot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(screenshot print format unicode)
target_link_libraries(screenshot moxnox)
target_link_libraries(screenshot libpng zlib)

install(TARGETS screenshot DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(seh-test src/seh-test.cpp)

target_compile_options(seh-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(seh-test print)
target_link_libraries(seh-test format)
target_link_libraries(seh-test moxnox)

install(TARGETS seh-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(seh2-test src/seh2-test.cpp)

    target_compile_options(seh2-test PRIVATE -fno-exceptions -fms-extensions ${USERSPACE_CFLAGS})

    target_link_libraries(seh2-test print-ms)

    install(TARGETS seh2-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)
endif()

###

add_executable(file-test src/file-test.cpp)

target_compile_options(file-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(file-test PUBLIC -mcrtdll=ucrtbase)
endif()

target_link_libraries(file-test print)
target_link_libraries(file-test format)

install(TARGETS file-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(crt-test src/crt-test.cpp)

target_compile_options(crt-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(crt-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS crt-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(regex-test src/regex-test.cpp)

target_compile_options(regex-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(regex-test posixshim)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(regex-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS regex-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(glob-test src/glob-test.cpp)

target_compile_options(glob-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(glob-test posixshim)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(glob-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS glob-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ps src/ps.cpp)

target_compile_options(ps PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(ps print unique_handle format list)
target_link_libraries(ps moxnox)

install(TARGETS ps DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(kill src/kill.cpp)

target_compile_options(kill PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(kill posixshim)
target_link_libraries(kill moxnox)

install(TARGETS kill DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(killall src/killall.cpp)

target_compile_options(killall PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(killall print format unique_handle unicode errno posixshim)
target_link_libraries(killall moxnox)

install(TARGETS killall DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(reboot src/reboot.cpp)

target_compile_options(reboot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(reboot print)
target_link_libraries(reboot format)
target_link_libraries(reboot moxnox)

install(TARGETS reboot DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(halt src/halt.cpp)

target_compile_options(halt PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(halt print)
target_link_libraries(halt format)
target_link_libraries(halt moxnox)

install(TARGETS halt DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mv src/mv.cpp)
target_sources(mv PUBLIC FILE_SET CXX_MODULES FILES src/copy.cpp)

target_compile_options(mv PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mv print)
target_link_libraries(mv format unique_handle list unicode)
target_link_libraries(mv moxnox)

install(TARGETS mv DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(cp src/cp.cpp)
target_sources(cp PUBLIC FILE_SET CXX_MODULES FILES src/copy.cpp)

target_compile_options(cp PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(cp print)
target_link_libraries(cp format unique_handle list unicode)
target_link_libraries(cp moxnox)

install(TARGETS cp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(mount src/mount.cpp)

target_compile_options(mount PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mount print)
target_link_libraries(mount format crt unique_handle unicode)
target_link_libraries(mount moxnox)

install(TARGETS mount DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(umount src/umount.cpp)

target_compile_options(umount PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(umount print)
target_link_libraries(umount format unique_handle unicode)
target_link_libraries(umount moxnox)

install(TARGETS umount DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(sock-test src/sock-test.cpp)

target_compile_options(sock-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(sock-test moxnox print ipv6 errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(sock-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS sock-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(udp-server-test src/udp-server-test.cpp)

target_compile_options(udp-server-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(udp-server-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(udp-server-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS udp-server-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(udp-client-test src/udp-client-test.cpp)

target_compile_options(udp-client-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(udp-client-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(udp-client-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS udp-client-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(getaddrinfo-test src/getaddrinfo-test.cpp)

target_compile_options(getaddrinfo-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(getaddrinfo-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(getaddrinfo-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS getaddrinfo-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tcp-client-test src/tcp-client-test.cpp)

target_compile_options(tcp-client-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tcp-client-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tcp-client-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tcp-client-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tcp-server-test src/tcp-server-test.cpp)

target_compile_options(tcp-server-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tcp-server-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tcp-server-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tcp-server-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(httpd-test src/httpd-test.cpp)

target_compile_options(httpd-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(httpd-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(httpd-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS httpd-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(exc-test src/exc-test.cpp)

target_compile_options(exc-test PRIVATE ${USERSPACE_CFLAGS})
target_link_libraries(exc-test print-exc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(exc-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS exc-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ifconfig src/ifconfig.cpp)

target_compile_options(ifconfig PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ifconfig moxnox print unique_handle ipv6)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ifconfig PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS ifconfig DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ping src/ping.cpp)

target_compile_options(ping PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ping print unique_handle ipv6 errno)
target_link_libraries(ping moxnox)
target_link_libraries(ping posixshim)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ping PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS ping DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(telnetd src/telnetd.cpp)

target_compile_options(telnetd PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(telnetd moxnox print errno unique_handle unicode posixshim)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(telnetd PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS telnetd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(rtti-test src/rtti-test.cpp)

target_compile_options(rtti-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -frtti)
target_link_libraries(rtti-test print-rtti)
target_link_options(rtti-test PRIVATE -Wl,--allow-multiple-definition) # FIXME

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(rtti-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS rtti-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tls-test src/tls-test.cpp)

target_compile_options(tls-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tls-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tls-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tls-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tls2-test src/tls2-test.cpp)

target_compile_options(tls2-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tls2-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tls2-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tls2-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(condvar-test src/condvar-test.cpp)

target_compile_options(condvar-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(condvar-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(condvar-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS condvar-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(apc-test src/apc-test.cpp)

target_compile_options(apc-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(apc-test moxnox print unique_handle)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(apc-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS apc-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

add_executable(signal-test src/signal-test.cpp)

target_compile_options(signal-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(signal-test moxnox print unique_handle posixshim)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(signal-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS signal-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(keyboard-test src/keyboard-test.cpp)

target_compile_options(keyboard-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(keyboard-test moxnox print evdev unique_handle)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(keyboard-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS keyboard-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mouse-test src/mouse-test.cpp)

target_compile_options(mouse-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(mouse-test moxnox print evdev unique_handle)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(mouse-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS mouse-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(stat src/stat.cpp)

target_compile_options(stat PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(stat moxnox print errno posixshim)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(stat PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS stat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(readline-test src/readline-test.cpp)

target_compile_options(readline-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(readline-test moxnox readline)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(readline-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS readline-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(setjmp-test src/setjmp-test.cpp)

target_compile_options(setjmp-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(setjmp-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS setjmp-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(popen-test src/popen-test.cpp)

target_compile_options(popen-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(popen-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS popen-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ntqo-test src/ntqo-test.cpp)

target_compile_options(ntqo-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ntqo-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ntqo-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS ntqo-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)
