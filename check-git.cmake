function(check_git_version)
    execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (EXISTS ${CMAKE_BINARY_DIR}/git-state.txt)
        file(STRINGS ${CMAKE_BINARY_DIR}/git-state.txt content)
        list(GET content 0 GIT_HASH_CACHE)
    endif()

    if(NOT DEFINED GIT_HASH_CACHE)
        set(GIT_HASH_CACHE "INVALID")
    endif()

    if(NOT GIT_HASH STREQUAL GIT_HASH_CACHE OR NOT EXISTS ${CMAKE_BINARY_DIR}/git/git_version.h)
        file(WRITE ${CMAKE_BINARY_DIR}/git-state.txt ${GIT_HASH})

        configure_file(${CMAKE_CURRENT_LIST_DIR}/git_version.h.in ${CMAKE_BINARY_DIR}/git/git_version.h @ONLY)
    endif()
endfunction()

function(check_git_setup)
    add_custom_target(always_check_git
        COMMAND ${CMAKE_COMMAND}
        -DRUN_CHECK_GIT_VERSION=1
        -P ${CMAKE_CURRENT_LIST_DIR}/check-git.cmake
        BYPRODUCTS ${CMAKE_BINARY_DIR}/git/git_version.h
    )

    check_git_version()
endfunction()

if(RUN_CHECK_GIT_VERSION)
    check_git_version()
endif ()
