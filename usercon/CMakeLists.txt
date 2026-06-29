cmake_minimum_required(VERSION 3.28)

project(usercon)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

add_executable(usercon src/usercon.cpp)

target_sources(usercon PUBLIC FILE_SET CXX_MODULES FILES
    src/text.cpp
    src/keyboard.cpp
    src/emoji.cpp)

target_compile_options(usercon PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(usercon ${LIBGCC})
target_link_libraries(usercon moxnox format rbtree unique_handle list vt100 unicode cp437 evdev pagesize timepage print)
target_link_libraries(usercon harfbuzz)
target_link_libraries(usercon freetype_module)

install(TARGETS usercon DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

# FIXME - put this in a better place in repo
install(FILES ../kernel/console.ttf DESTINATION ${CMAKE_INSTALL_PREFIX}/share/fonts COMPONENT core)

install(FILES console-bold.ttf DESTINATION ${CMAKE_INSTALL_PREFIX}/share/fonts COMPONENT core)
install(FILES console-italic.ttf DESTINATION ${CMAKE_INSTALL_PREFIX}/share/fonts COMPONENT core)
install(FILES console-bold-italic.ttf DESTINATION ${CMAKE_INSTALL_PREFIX}/share/fonts COMPONENT core)
install(FILES emoji.ttf DESTINATION ${CMAKE_INSTALL_PREFIX}/share/fonts COMPONENT core)
