if(NOT DEFINED GNULIB_LIB_DIR)
    set(GNULIB_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib)
endif()

file(READ ${GNULIB_LIB_DIR}/arg-nonnull.h GNULIB_SNIPPET_ARG_NONNULL)
file(READ ${GNULIB_LIB_DIR}/c++defs.h GNULIB_SNIPPET_CXXDEFS)
file(READ ${GNULIB_LIB_DIR}/warn-on-use.h GNULIB_SNIPPET_WARN_ON_USE)
file(READ ${GNULIB_LIB_DIR}/_Noreturn.h GNULIB_SNIPPET_NORETURN)

function(gnulib_header template output)
    if(NOT IS_ABSOLUTE ${template})
        set(template ${CMAKE_CURRENT_SOURCE_DIR}/${template})
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${template})
    file(READ ${template} content)
    string(REPLACE "/* The definition of _GL_ARG_NONNULL is copied here.  */"
        "/* The definition of _GL_ARG_NONNULL is copied here.  */\n${GNULIB_SNIPPET_ARG_NONNULL}"
        content "${content}")
    string(REPLACE "/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */"
        "/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */\n${GNULIB_SNIPPET_CXXDEFS}"
        content "${content}")
    string(REPLACE "/* The definition of _GL_WARN_ON_USE is copied here.  */"
        "/* The definition of _GL_WARN_ON_USE is copied here.  */\n${GNULIB_SNIPPET_WARN_ON_USE}"
        content "${content}")
    string(REPLACE "/* The definition of _Noreturn is copied here.  */"
        "/* The definition of _Noreturn is copied here.  */\n${GNULIB_SNIPPET_NORETURN}"
        content "${content}")
    file(CONFIGURE OUTPUT ${output} CONTENT "${content}" @ONLY)
endfunction()
