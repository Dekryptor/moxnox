cmake_minimum_required(VERSION 3.28)

project(shell)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_library(shelllib STATIC)
target_sources(shelllib PUBLIC FILE_SET CXX_MODULES FILES
    src/shelllib.cpp
    src/print.cpp
)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

target_compile_options(shelllib PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(shelllib format)

###

set(SRC_FILES src/shell.cpp)

add_executable(shell ${SRC_FILES})

target_compile_options(shell PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    target_compile_options(shell PRIVATE -masm=intel)
endif()

target_link_options(shell PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(shell shelllib unicode unique_handle signal cmdline list pagesize)
target_link_libraries(shell moxnox)
target_link_libraries(shell format)

install(TARGETS shell DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(echo src/echo.cpp)

target_compile_options(echo PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(echo PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(echo shelllib unicode cmdline)
target_link_libraries(echo moxnox)

install(TARGETS echo DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(ls src/ls.cpp)

target_compile_options(ls PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ls PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(ls shelllib)
target_link_libraries(ls format list cmdline getopt)
target_link_libraries(ls moxnox)

install(TARGETS ls DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(cat src/cat.cpp)

target_compile_options(cat PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(cat PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(cat shelllib)
target_link_libraries(cat format cmdline)
target_link_libraries(cat moxnox)

install(TARGETS cat DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(pwd src/pwd.cpp)

target_compile_options(pwd PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(pwd PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(pwd shelllib unicode format)
target_link_libraries(pwd moxnox)

install(TARGETS pwd DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(yes src/yes.cpp)

target_compile_options(yes PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(yes PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(yes shelllib format)
target_link_libraries(yes moxnox)

install(TARGETS yes DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(mkdir src/mkdir.cpp)

target_compile_options(mkdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(mkdir PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(mkdir shelllib)
target_link_libraries(mkdir format cmdline)
target_link_libraries(mkdir moxnox)

install(TARGETS mkdir DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(touch src/touch.cpp)

target_compile_options(touch PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(touch PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(touch shelllib)
target_link_libraries(touch format cmdline)
target_link_libraries(touch moxnox)

install(TARGETS touch DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(rm src/rm.cpp)

target_compile_options(rm PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(rm PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(rm shelllib)
target_link_libraries(rm format cmdline)
target_link_libraries(rm moxnox)

install(TARGETS rm DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(rmdir src/rmdir.cpp)

target_compile_options(rmdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(rmdir PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(rmdir shelllib)
target_link_libraries(rmdir format cmdline)
target_link_libraries(rmdir moxnox)

install(TARGETS rmdir DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(screenshot src/screenshot.cpp)

target_compile_options(screenshot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(screenshot PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(screenshot shelllib)
target_link_libraries(screenshot format unicode cmdline)
target_link_libraries(screenshot moxnox)
target_link_libraries(screenshot libpng zlib)

install(TARGETS screenshot DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(seh-test src/seh-test.cpp)

target_compile_options(seh-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(seh-test PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(seh-test shelllib)
target_link_libraries(seh-test format)
target_link_libraries(seh-test moxnox)

install(TARGETS seh-test DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(ps src/ps.cpp)

target_compile_options(ps PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ps PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(ps shelllib unique_handle format list)
target_link_libraries(ps moxnox)

install(TARGETS ps DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(kill src/kill.cpp)

target_compile_options(kill PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(kill PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(kill shelllib cmdline format unique_handle)
target_link_libraries(kill moxnox)

install(TARGETS kill DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(killall src/killall.cpp)

target_compile_options(killall PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(killall PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(killall shelllib cmdline format unique_handle)
target_link_libraries(killall moxnox)

install(TARGETS killall DESTINATION ${CMAKE_INSTALL_PREFIX})

###

add_executable(reboot src/reboot.cpp)

target_compile_options(reboot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(reboot PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(reboot shelllib)
target_link_libraries(reboot format)
target_link_libraries(reboot moxnox)

install(TARGETS reboot DESTINATION ${CMAKE_INSTALL_PREFIX})
