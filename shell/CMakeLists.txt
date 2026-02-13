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

target_link_options(shell PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(shell print unicode unique_handle signal cmdline list pagesize env)
target_link_libraries(shell moxnox)
target_link_libraries(shell format)

install(TARGETS shell DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(echo src/echo.cpp)

target_compile_options(echo PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(echo PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(echo print unicode cmdline format)
target_link_libraries(echo moxnox)

install(TARGETS echo DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(ls src/ls.cpp)

target_compile_options(ls PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ls PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(ls print)
target_link_libraries(ls format list cmdline getopt)
target_link_libraries(ls moxnox)

install(TARGETS ls DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(cat src/cat.cpp)

target_compile_options(cat PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(cat PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(cat print)
target_link_libraries(cat format cmdline)
target_link_libraries(cat moxnox)

install(TARGETS cat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(pwd src/pwd.cpp)

target_compile_options(pwd PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(pwd PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(pwd print unicode format)
target_link_libraries(pwd moxnox)

install(TARGETS pwd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(yes src/yes.cpp)

target_compile_options(yes PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(yes PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(yes print format)
target_link_libraries(yes moxnox)

install(TARGETS yes DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mkdir src/mkdir.cpp)

target_compile_options(mkdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(mkdir PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(mkdir print)
target_link_libraries(mkdir format cmdline)
target_link_libraries(mkdir moxnox)

install(TARGETS mkdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(touch src/touch.cpp)

target_compile_options(touch PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(touch PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(touch print)
target_link_libraries(touch format cmdline)
target_link_libraries(touch moxnox)

install(TARGETS touch DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(rm src/rm.cpp)

target_compile_options(rm PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(rm PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(rm print)
target_link_libraries(rm format cmdline)
target_link_libraries(rm moxnox)

install(TARGETS rm DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(rmdir src/rmdir.cpp)

target_compile_options(rmdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(rmdir PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(rmdir print)
target_link_libraries(rmdir format cmdline)
target_link_libraries(rmdir moxnox)

install(TARGETS rmdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(screenshot src/screenshot.cpp)

target_compile_options(screenshot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(screenshot PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(screenshot print)
target_link_libraries(screenshot format unicode cmdline)
target_link_libraries(screenshot moxnox)
target_link_libraries(screenshot libpng zlib)

install(TARGETS screenshot DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(seh-test src/seh-test.cpp)

target_compile_options(seh-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(seh-test PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(seh-test print)
target_link_libraries(seh-test format)
target_link_libraries(seh-test moxnox)

install(TARGETS seh-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(file-test src/file-test.cpp)

target_compile_options(file-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(file-test PUBLIC -mcrtdll=ucrtbase)
endif()

target_link_libraries(file-test print)
target_link_libraries(file-test format)
target_link_libraries(file-test moxnox)

install(TARGETS file-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(ps src/ps.cpp)

target_compile_options(ps PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ps PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(ps print unique_handle format list)
target_link_libraries(ps moxnox)

install(TARGETS ps DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(kill src/kill.cpp)

target_compile_options(kill PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(kill PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(kill print cmdline format unique_handle)
target_link_libraries(kill moxnox)

install(TARGETS kill DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(killall src/killall.cpp)

target_compile_options(killall PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(killall PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(killall print cmdline format unique_handle)
target_link_libraries(killall moxnox)

install(TARGETS killall DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(reboot src/reboot.cpp)

target_compile_options(reboot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(reboot PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(reboot print)
target_link_libraries(reboot format)
target_link_libraries(reboot moxnox)

install(TARGETS reboot DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mv src/mv.cpp)
target_sources(mv PUBLIC FILE_SET CXX_MODULES FILES src/copy.cpp)

target_compile_options(mv PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(mv PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(mv print)
target_link_libraries(mv format cmdline unique_handle list)
target_link_libraries(mv moxnox)

install(TARGETS mv DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(cp src/cp.cpp)
target_sources(cp PUBLIC FILE_SET CXX_MODULES FILES src/copy.cpp)

target_compile_options(cp PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(cp PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(cp print)
target_link_libraries(cp format cmdline unique_handle getopt list)
target_link_libraries(cp moxnox)

install(TARGETS cp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mount src/mount.cpp)

target_compile_options(mount PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(mount PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(mount print)
target_link_libraries(mount format cmdline getopt crt unique_handle unicode)
target_link_libraries(mount moxnox)

install(TARGETS mount DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(umount src/umount.cpp)

target_compile_options(umount PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(umount PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(umount print)
target_link_libraries(umount format cmdline unique_handle unicode getopt)
target_link_libraries(umount moxnox)

install(TARGETS umount DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(exc-test src/exc-test.cpp)

target_compile_options(exc-test PRIVATE ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(exc-test PUBLIC -mcrtdll=ucrtbase)
endif()

target_link_libraries(exc-test moxnox)

install(TARGETS exc-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
