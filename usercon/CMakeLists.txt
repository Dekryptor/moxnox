cmake_minimum_required(VERSION 3.28)

project(usercon)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

add_executable(usercon src/usercon.cpp)

target_sources(usercon PUBLIC FILE_SET CXX_MODULES FILES
    src/print.cpp)

target_compile_options(usercon PRIVATE -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -fno-exceptions -fno-rtti -masm=intel -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -gdwarf-5)
target_link_options(usercon PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup)

target_link_libraries(usercon harfbuzz)
target_link_libraries(usercon -lntdll ntdllcrt -lgcc)
target_link_libraries(usercon format_sse rbtree_sse unique_handle list_sse vt100_sse signal)
target_link_libraries(usercon freetype)

install(TARGETS usercon DESTINATION ${CMAKE_INSTALL_PREFIX}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL)

# FIXME - put this in a better place
install(FILES ../kernel/console.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})

install(FILES console-bold.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES console-italic.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES console-bold-italic.ttf DESTINATION ${CMAKE_INSTALL_PREFIX})
