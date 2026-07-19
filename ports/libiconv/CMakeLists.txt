set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/lib ${GEN}/include)

add_custom_command(
    OUTPUT ${GEN}/lib/aliases.h ${GEN}/lib/canonical.h ${GEN}/lib/canonical_local.h
    COMMAND sh -c "cd '${GEN}/lib' && cc '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/genaliases.c' -o genaliases && ./genaliases aliases.gperf canonical.sh canonical_local.sh && gperf -m 10 aliases.gperf | sed -e 's/^\\(const struct alias \\)/static \\1/' > aliases.h && sh canonical.sh > canonical.h && sh canonical_local.sh > canonical_local.h && rm -f genaliases aliases.gperf canonical.sh canonical_local.sh"
    DEPENDS repo/lib/genaliases.c repo/lib/encodings.def repo/lib/encodings_local.def
    VERBATIM
)

foreach(extra aix osf1 dos zos)
    string(TOUPPER ${extra} extra_upper)
    add_custom_command(
        OUTPUT ${GEN}/lib/aliases_${extra}.h ${GEN}/lib/canonical_${extra}.h
        COMMAND sh -c "cd '${GEN}/lib' && cc -DUSE_${extra_upper} '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/genaliases2.c' -o genaliases2_${extra} && ./genaliases2_${extra} ${extra} aliases_${extra}.h canonical_${extra}.h && rm -f genaliases2_${extra}"
        DEPENDS repo/lib/genaliases2.c repo/lib/encodings_${extra}.def
        VERBATIM
    )
    list(APPEND ICONV_EXTRA_TABLES
        ${GEN}/lib/aliases_${extra}.h ${GEN}/lib/canonical_${extra}.h)
endforeach()

add_custom_command(
    OUTPUT ${GEN}/lib/flags.h
    COMMAND sh -c "cd '${GEN}/lib' && cc '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/genflags.c' -o genflags && ./genflags > flags.h && rm -f genflags"
    DEPENDS repo/lib/genflags.c repo/lib/converters.h repo/lib/encodings.def
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/translit.h
    COMMAND sh -c "cd '${GEN}/lib' && cc '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/gentranslit.c' -o gentranslit && ./gentranslit < '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/translit.def' > translit.h && rm -f gentranslit"
    DEPENDS repo/lib/gentranslit.c repo/lib/translit.def
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/include/localcharset.h
    COMMAND sh -c "sed -e 's/extern \\([^\"]\\)/extern LIBCHARSET_DLL_EXPORTED \\1/' -e '/#define _LOCALCHARSET_H/r ${CMAKE_CURRENT_SOURCE_DIR}/repo/libcharset/include/export.h' '${CMAKE_CURRENT_SOURCE_DIR}/repo/libcharset/include/localcharset.h.in' | sed -e 's/@HAVE_VISIBILITY@/1/' > '${GEN}/include/localcharset.h'"
    DEPENDS repo/libcharset/include/localcharset.h.in repo/libcharset/include/export.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/include/iconv.h
    COMMAND sh -c "sed -e 's/@DLL_VARIABLE@//' -e 's/@EILSEQ@//' -e 's/@ICONV_CONST@//' -e 's/@USE_MBSTATE_T@/1/' -e 's/@BROKEN_WCHAR_H@/0/' -e 's/@HAVE_WCHAR_T@/1/' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/iconv.h.in' > '${GEN}/include/iconv.h'"
    DEPENDS repo/include/iconv.h.in
    VERBATIM
)

add_library(iconv SHARED
    repo/lib/iconv.c
    repo/libcharset/lib/localcharset.c
    ${GEN}/lib/aliases.h
    ${GEN}/lib/canonical.h
    ${GEN}/lib/canonical_local.h
    ${GEN}/lib/flags.h
    ${GEN}/lib/translit.h
    ${ICONV_EXTRA_TABLES}
    ${GEN}/include/iconv.h
    ${GEN}/include/localcharset.h
)

set_target_properties(iconv PROPERTIES OUTPUT_NAME "iconv-2" PREFIX "lib")

target_compile_options(iconv PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default -std=gnu17)

target_compile_options(iconv PRIVATE
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-implicit-fallthrough
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-type-limits
    -Wno-unused-const-variable
    -Wno-misleading-indentation
    -Wno-parentheses-equality
    -Wno-deprecated-non-prototype
)

target_compile_definitions(iconv PRIVATE -DHAVE_CONFIG_H -DBUILDING_LIBICONV -DBUILDING_LIBCHARSET)
target_include_directories(iconv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/include
    ${GEN}/lib
    repo/lib
    repo/libcharset/include
)

install(TARGETS iconv DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)
