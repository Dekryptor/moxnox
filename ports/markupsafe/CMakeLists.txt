project(markupsafe NONE)

install(DIRECTORY repo/src/markupsafe/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13/site-packages/markupsafe
    PATTERN "__pycache__" EXCLUDE
    PATTERN "_speedups.c" EXCLUDE
    PATTERN "_speedups.pyi" EXCLUDE)

add_custom_target(markupsafe)
