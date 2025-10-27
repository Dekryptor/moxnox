cmake_minimum_required(VERSION 3.28)

project(usercon)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

add_executable(usercon src/usercon.cpp)

target_sources(usercon PUBLIC FILE_SET CXX_MODULES FILES
    src/print.cpp
    src/text.cpp
    src/keyboard.cpp)

target_compile_options(usercon PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(usercon PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(usercon moxnox -lgcc)
target_link_libraries(usercon format rbtree unique_handle list vt100 signal unicode cp437 cmdline evdev)
target_link_libraries(usercon harfbuzz)
target_link_libraries(usercon freetype)

install(TARGETS usercon DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL)

# FIXME - put this in a better place
install(FILES ../kernel/console.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})

install(FILES console-bold.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES console-italic.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES console-bold-italic.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})
