set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/libarchive ${GEN}/tar ${GEN}/libarchive_fe ${GEN}/cpio)

add_custom_command(
    OUTPUT ${GEN}/libarchive/archive.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/libarchive/archive.h ${GEN}/libarchive/archive.h
    COMMAND sed -i
        -e [[s@#if ((defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__)) && (!defined LIBARCHIVE_STATIC)@#if ((defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__) || defined(__moxnox__)) \&\& (!defined LIBARCHIVE_STATIC)@g]]
        ${GEN}/libarchive/archive.h
    DEPENDS repo/libarchive/archive.h
    VERBATIM
)
list(APPEND LA_GEN_OUTPUTS ${GEN}/libarchive/archive.h)

add_custom_command(
    OUTPUT ${GEN}/libarchive/archive_entry.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/libarchive/archive_entry.h ${GEN}/libarchive/archive_entry.h
    COMMAND sed -i
        -e [[s@#if ((defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__)) && (!defined LIBARCHIVE_STATIC)@#if ((defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__) || defined(__moxnox__)) \&\& (!defined LIBARCHIVE_STATIC)@g]]
        ${GEN}/libarchive/archive_entry.h
    DEPENDS repo/libarchive/archive_entry.h
    VERBATIM
)
list(APPEND LA_GEN_OUTPUTS ${GEN}/libarchive/archive_entry.h)

add_custom_command(
    OUTPUT ${GEN}/cpio/cpio.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/cpio/cpio.c ${GEN}/cpio/cpio.c
    COMMAND sed -i
        -e [[s@archive_write_add_filter_lz4(cpio->archive)@(ARCHIVE_FATAL)@g]]
        -e [[s@archive_write_add_filter_lzop(cpio->archive)@(ARCHIVE_FATAL)@g]]
        -e [[s@archive_write_add_filter_lrzip(cpio->archive)@(ARCHIVE_FATAL)@g]]
        -e [[s@archive_write_add_filter_grzip(cpio->archive)@(ARCHIVE_FATAL)@g]]
        ${GEN}/cpio/cpio.c
    DEPENDS repo/cpio/cpio.c
    VERBATIM
)

list(APPEND LA_GEN_OUTPUTS ${GEN}/cpio/cpio.c)

add_custom_command(
    OUTPUT ${GEN}/libarchive/archive_read_support_filter_all.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/libarchive/archive_read_support_filter_all.c
            ${GEN}/libarchive/archive_read_support_filter_all.c
    COMMAND sed -i
            -e [[/archive_read_support_filter_lz4(a);/d]]
            -e [[/archive_read_support_filter_lzop(a);/d]]
            -e [[/archive_read_support_filter_lrzip(a);/d]]
            -e [[/archive_read_support_filter_grzip(a);/d]]
            ${GEN}/libarchive/archive_read_support_filter_all.c
    DEPENDS repo/libarchive/archive_read_support_filter_all.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/libarchive/archive_read_support_filter_by_code.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/libarchive/archive_read_support_filter_by_code.c
            ${GEN}/libarchive/archive_read_support_filter_by_code.c
    COMMAND sed -i
            -e [[/case ARCHIVE_FILTER_LZ4:/,+1d]]
            -e [[/case ARCHIVE_FILTER_LZOP:/,+1d]]
            -e [[/case ARCHIVE_FILTER_LRZIP:/,+1d]]
            -e [[/case ARCHIVE_FILTER_GRZIP:/,+1d]]
            ${GEN}/libarchive/archive_read_support_filter_by_code.c
    DEPENDS repo/libarchive/archive_read_support_filter_by_code.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tar/cmdline.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/tar/cmdline.c ${GEN}/tar/cmdline.c
    COMMAND sed -i -e [[/use-compress-program/d]] ${GEN}/tar/cmdline.c
    DEPENDS repo/tar/cmdline.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tar/creation_set.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/tar/creation_set.c ${GEN}/tar/creation_set.c
    COMMAND sed -i
            -e [[/archive_read_support_filter_program(a,/,+1d]]
            -e [[/if (cset->filters\[i\].program)$/,+3d]]
            ${GEN}/tar/creation_set.c
    DEPENDS repo/tar/creation_set.c
    VERBATIM
)

foreach(reg archive_write_add_filter.c archive_write_add_filter_by_name.c
            archive_write_set_format_filter_by_ext.c)
    add_custom_command(
        OUTPUT ${GEN}/libarchive/${reg}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/libarchive/${reg} ${GEN}/libarchive/${reg}
        COMMAND sed -i
                -e [[/archive_write_add_filter_lz4/d]]
                -e [[/archive_write_add_filter_lzop/d]]
                -e [[/archive_write_add_filter_lrzip/d]]
                -e [[/archive_write_add_filter_grzip/d]]
                ${GEN}/libarchive/${reg}
        DEPENDS repo/libarchive/${reg}
        VERBATIM
    )
endforeach()

list(APPEND LA_GEN_OUTPUTS
    ${GEN}/libarchive/archive_read_support_filter_all.c
    ${GEN}/libarchive/archive_read_support_filter_by_code.c
    ${GEN}/libarchive/archive_write_add_filter.c
    ${GEN}/libarchive/archive_write_add_filter_by_name.c
    ${GEN}/libarchive/archive_write_set_format_filter_by_ext.c
    ${GEN}/tar/cmdline.c
    ${GEN}/tar/creation_set.c)

add_custom_target(libarchive_gen DEPENDS ${LA_GEN_OUTPUTS})

add_library(archive SHARED
    repo/libarchive/archive_acl.c
    repo/libarchive/archive_blake2sp_ref.c
    repo/libarchive/archive_blake2s_ref.c
    repo/libarchive/archive_check_magic.c
    repo/libarchive/archive_cmdline.c
    repo/libarchive/archive_cryptor.c
    repo/libarchive/archive_digest.c
    repo/libarchive/archive_entry.c
    repo/libarchive/archive_entry_copy_bhfi.c
    repo/libarchive/archive_entry_copy_stat.c
    repo/libarchive/archive_entry_link_resolver.c
    repo/libarchive/archive_entry_sparse.c
    repo/libarchive/archive_entry_stat.c
    repo/libarchive/archive_entry_strmode.c
    repo/libarchive/archive_entry_xattr.c
    repo/libarchive/archive_hmac.c
    repo/libarchive/archive_match.c
    repo/libarchive/archive_options.c
    repo/libarchive/archive_pack_dev.c
    repo/libarchive/archive_parse_date.c
    repo/libarchive/archive_pathmatch.c
    repo/libarchive/archive_ppmd7.c
    repo/libarchive/archive_ppmd8.c
    repo/libarchive/archive_random.c
    repo/libarchive/archive_rb.c
    repo/libarchive/archive_read_add_passphrase.c
    repo/libarchive/archive_read.c
    repo/libarchive/archive_read_data_into_fd.c
    repo/libarchive/archive_read_disk_entry_from_file.c
    repo/libarchive/archive_read_disk_posix.c
    repo/libarchive/archive_read_disk_set_standard_lookup.c
    repo/libarchive/archive_read_extract2.c
    repo/libarchive/archive_read_extract.c
    repo/libarchive/archive_read_open_fd.c
    repo/libarchive/archive_read_open_file.c
    repo/libarchive/archive_read_open_filename.c
    repo/libarchive/archive_read_open_memory.c
    repo/libarchive/archive_read_set_format.c
    repo/libarchive/archive_read_set_options.c
    ${GEN}/libarchive/archive_read_support_filter_all.c
    ${GEN}/libarchive/archive_read_support_filter_by_code.c
    repo/libarchive/archive_read_support_filter_bzip2.c
    repo/libarchive/archive_read_support_filter_compress.c
    repo/libarchive/archive_read_support_filter_gzip.c
    repo/libarchive/archive_read_support_filter_none.c
    repo/libarchive/archive_read_support_filter_rpm.c
    repo/libarchive/archive_read_support_filter_uu.c
    repo/libarchive/archive_read_support_filter_xz.c
    repo/libarchive/archive_read_support_filter_zstd.c
    repo/libarchive/archive_read_support_format_7zip.c
    repo/libarchive/archive_read_support_format_all.c
    repo/libarchive/archive_read_support_format_ar.c
    repo/libarchive/archive_read_support_format_by_code.c
    repo/libarchive/archive_read_support_format_cab.c
    repo/libarchive/archive_read_support_format_cpio.c
    repo/libarchive/archive_read_support_format_empty.c
    repo/libarchive/archive_read_support_format_iso9660.c
    repo/libarchive/archive_read_support_format_lha.c
    repo/libarchive/archive_read_support_format_mtree.c
    repo/libarchive/archive_read_support_format_rar5.c
    repo/libarchive/archive_read_support_format_rar.c
    repo/libarchive/archive_read_support_format_raw.c
    repo/libarchive/archive_read_support_format_tar.c
    repo/libarchive/archive_read_support_format_warc.c
    repo/libarchive/archive_read_support_format_xar.c
    repo/libarchive/archive_read_support_format_zip.c
    repo/libarchive/archive_string.c
    repo/libarchive/archive_string_sprintf.c
    repo/libarchive/archive_time.c
    repo/libarchive/archive_util.c
    repo/libarchive/archive_version_details.c
    repo/libarchive/archive_virtual.c
    repo/libarchive/archive_write_add_filter_b64encode.c
    ${GEN}/libarchive/archive_write_add_filter_by_name.c
    ${GEN}/libarchive/archive_write_add_filter.c
    repo/libarchive/archive_write_add_filter_bzip2.c
    repo/libarchive/archive_write_add_filter_compress.c
    repo/libarchive/archive_write_add_filter_gzip.c
    repo/libarchive/archive_write_add_filter_none.c
    repo/libarchive/archive_write_add_filter_uuencode.c
    repo/libarchive/archive_write_add_filter_xz.c
    repo/libarchive/archive_write_add_filter_zstd.c
    repo/libarchive/archive_write.c
    repo/libarchive/archive_write_disk_posix.c
    repo/libarchive/archive_write_disk_set_standard_lookup.c
    repo/libarchive/archive_write_open_fd.c
    repo/libarchive/archive_write_open_file.c
    repo/libarchive/archive_write_open_filename.c
    repo/libarchive/archive_write_open_memory.c
    repo/libarchive/archive_write_set_format_7zip.c
    repo/libarchive/archive_write_set_format_ar.c
    repo/libarchive/archive_write_set_format_by_name.c
    repo/libarchive/archive_write_set_format.c
    repo/libarchive/archive_write_set_format_cpio_binary.c
    repo/libarchive/archive_write_set_format_cpio.c
    repo/libarchive/archive_write_set_format_cpio_newc.c
    repo/libarchive/archive_write_set_format_cpio_odc.c
    ${GEN}/libarchive/archive_write_set_format_filter_by_ext.c
    repo/libarchive/archive_write_set_format_gnutar.c
    repo/libarchive/archive_write_set_format_iso9660.c
    repo/libarchive/archive_write_set_format_mtree.c
    repo/libarchive/archive_write_set_format_pax.c
    repo/libarchive/archive_write_set_format_raw.c
    repo/libarchive/archive_write_set_format_shar.c
    repo/libarchive/archive_write_set_format_ustar.c
    repo/libarchive/archive_write_set_format_v7tar.c
    repo/libarchive/archive_write_set_format_warc.c
    repo/libarchive/archive_write_set_format_xar.c
    repo/libarchive/archive_write_set_format_zip.c
    repo/libarchive/archive_write_set_options.c
    repo/libarchive/archive_write_set_passphrase.c
    repo/libarchive/xxhash.c
)
add_dependencies(archive libarchive_gen)

target_include_directories(archive PRIVATE
    repo/libarchive
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(archive PUBLIC repo/libarchive)

target_compile_definitions(archive PRIVATE HAVE_CONFIG_H __LIBARCHIVE_BUILD MOXNOX)

target_compile_options(archive PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-function
    -Wno-unused-variable
    "SHELL:-include ${GEN}/libarchive/archive.h"
    "SHELL:-include ${GEN}/libarchive/archive_entry.h"
)

target_link_libraries(archive PRIVATE bzip2 liblzma zlib zstd)

install(TARGETS archive DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)

###

add_executable(bsdtar
    repo/tar/bsdtar.c
    ${GEN}/tar/cmdline.c
    ${GEN}/tar/creation_set.c
    repo/tar/read.c
    repo/tar/subst.c
    repo/tar/util.c
    repo/tar/write.c
    repo/libarchive_fe/lafe_err.c
    repo/libarchive_fe/line_reader.c
    repo/libarchive_fe/passphrase.c
)
add_dependencies(bsdtar libarchive_gen)

target_include_directories(bsdtar PRIVATE
    repo/libarchive
    repo/libarchive_fe
    repo/tar
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(bsdtar PRIVATE HAVE_CONFIG_H MOXNOX)

target_compile_options(bsdtar PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    "SHELL:-include ${GEN}/libarchive/archive.h"
)

install(CODE "file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin\")")
install(CODE "file(CREATE_LINK bsdtar.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/tar.exe\" SYMBOLIC)")

target_link_libraries(bsdtar PRIVATE archive moxnox)

install(TARGETS bsdtar DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(bsdcat
    repo/cat/bsdcat.c
    repo/cat/cmdline.c
    repo/libarchive_fe/lafe_err.c
)
add_dependencies(bsdcat libarchive_gen)

target_include_directories(bsdcat PRIVATE
    repo/libarchive
    repo/libarchive_fe
    repo/cat
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(bsdcat PRIVATE HAVE_CONFIG_H MOXNOX)

target_compile_options(bsdcat PRIVATE ${USERSPACE_CFLAGS})

target_link_libraries(bsdcat PRIVATE archive moxnox)

install(TARGETS bsdcat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(bsdcpio
    repo/cpio/cmdline.c
    ${GEN}/cpio/cpio.c
    repo/libarchive_fe/lafe_err.c
    repo/libarchive_fe/lafe_getline.c
    repo/libarchive_fe/line_reader.c
    repo/libarchive_fe/passphrase.c
)
add_dependencies(bsdcpio libarchive_gen)

target_include_directories(bsdcpio PRIVATE
    repo/libarchive
    repo/libarchive_fe
    repo/cpio
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(bsdcpio PRIVATE HAVE_CONFIG_H MOXNOX)

target_compile_options(bsdcpio PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-conversion
)

target_link_libraries(bsdcpio PRIVATE archive moxnox)

install(TARGETS bsdcpio DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(bsdunzip
    repo/unzip/bsdunzip.c
    repo/unzip/cmdline.c
    repo/libarchive_fe/lafe_err.c
    repo/libarchive_fe/lafe_getline.c
    repo/libarchive_fe/passphrase.c
)
add_dependencies(bsdunzip libarchive_gen)

target_include_directories(bsdunzip PRIVATE
    repo/libarchive
    repo/libarchive_fe
    repo/unzip
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(bsdunzip PRIVATE HAVE_CONFIG_H MOXNOX)

target_compile_options(bsdunzip PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-function
)

target_link_libraries(bsdunzip PRIVATE archive moxnox)

install(TARGETS bsdunzip DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_custom_target(libarchive)
add_dependencies(libarchive
    archive
    bsdtar
    bsdcat
    bsdcpio
    bsdunzip
)
