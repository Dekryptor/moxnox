project(mako NONE)

install(DIRECTORY repo/mako/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13/site-packages/mako
    PATTERN "__pycache__" EXCLUDE
    PATTERN "ext" EXCLUDE
    PATTERN "testing" EXCLUDE)

add_custom_target(mako)
