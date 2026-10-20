project(pyyaml NONE)

install(DIRECTORY repo/lib/yaml/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13/site-packages/yaml
    PATTERN "__pycache__" EXCLUDE)

add_custom_target(pyyaml)
