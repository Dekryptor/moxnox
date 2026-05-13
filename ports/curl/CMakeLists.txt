set(CURL_GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${CURL_GEN}/lib/curlx)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/rand.c _src)
string(REPLACE "#ifndef CURL_WINDOWS_UWP"
"#if !defined(CURL_WINDOWS_UWP) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/lib/rand.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/getenv.c _src)
string(REPLACE "#elif defined(_WIN32)"
"#elif defined(_WIN32) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/lib/getenv.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/curlx/fopen.c _src)
string(REPLACE "#ifdef _WIN32"
"#if defined(_WIN32) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/lib/curlx/fopen.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/setup-win32.h _src)
string(REPLACE "#  define USE_WINSOCK 2"
               "#  if !defined(MOXNOX)\n#    define USE_WINSOCK 2\n#  endif" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/lib/setup-win32.h" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/curlx/version_win32.h _src)
string(REPLACE "#ifdef CURL_WINDOWS_UWP"
               "#if defined(CURL_WINDOWS_UWP) || defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/lib/curlx/version_win32.h" CONTENT "${_src}")

file(MAKE_DIRECTORY "${CURL_GEN}/src")
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tool_filetime.c _src)
string(REPLACE "#if defined(_WIN32) && !defined(CURL_WINDOWS_UWP)"
               "#if defined(_WIN32) && !defined(CURL_WINDOWS_UWP) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/tool_filetime.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/terminal.c _src)
string(REPLACE "#elif defined(_WIN32) && !defined(CURL_WINDOWS_UWP)"
               "#elif defined(_WIN32) && !defined(CURL_WINDOWS_UWP) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/terminal.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tool_cb_wrt.c _src)
string(REPLACE "#ifdef _WIN32
static size_t win_console("
               "#if defined(_WIN32) && !defined(MOXNOX)
static size_t win_console(" _src "${_src}")
string(REPLACE "#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO"
               "#if defined(_WIN32) && !defined(MOXNOX)
  CONSOLE_SCREEN_BUFFER_INFO" _src "${_src}")
string(REPLACE "#ifdef _WIN32
  fhnd = _get_osfhandle("
               "#if defined(_WIN32) && !defined(MOXNOX)
  fhnd = _get_osfhandle(" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/tool_cb_wrt.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tool_doswin.c _src)
string(REPLACE "!defined(CURL_CA_SEARCH_SAFE)"
               "!defined(CURL_CA_SEARCH_SAFE) && !defined(MOXNOX)" _src "${_src}")
string(REPLACE "#ifndef CURL_WINDOWS_UWP"
               "#if !defined(CURL_WINDOWS_UWP) && !defined(MOXNOX)" _src "${_src}")
string(REPLACE "#endif /* !CURL_WINDOWS_UWP */"
               "#endif /* !CURL_WINDOWS_UWP && !MOXNOX */" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/tool_doswin.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tool_util.c _src)
string(REPLACE "#ifdef _WIN32"
               "#if defined(_WIN32) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/tool_util.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tool_getpass.c _src)
string(REPLACE "#ifdef _WIN32"
               "#if defined(_WIN32) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/tool_getpass.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tool_operate.c _src)
string(REPLACE "#elif !defined(CURL_WINDOWS_UWP) && !defined(CURL_DISABLE_CA_SEARCH)"
               "#elif !defined(CURL_WINDOWS_UWP) && !defined(CURL_DISABLE_CA_SEARCH) && !defined(MOXNOX)" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/tool_operate.c" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/tool_parsecfg.c _src)
string(REPLACE "#ifdef _WIN32
    else {"
               "#if defined(_WIN32) && !defined(MOXNOX)
    else {" _src "${_src}")
file(CONFIGURE OUTPUT "${CURL_GEN}/src/tool_parsecfg.c" CONTENT "${_src}")

add_library(libcurl SHARED
    repo/lib/curlx/base64.c
    repo/lib/curlx/basename.c
    repo/lib/curlx/dynbuf.c
    ${CURL_GEN}/lib/curlx/fopen.c
    repo/lib/curlx/inet_ntop.c
    repo/lib/curlx/inet_pton.c
    repo/lib/curlx/nonblock.c
    repo/lib/curlx/snprintf.c
    repo/lib/curlx/strcopy.c
    repo/lib/curlx/strdup.c
    repo/lib/curlx/strerr.c
    repo/lib/curlx/strparse.c
    repo/lib/curlx/timediff.c
    repo/lib/curlx/timeval.c
    repo/lib/curlx/wait.c
    repo/lib/curlx/warnless.c
    repo/lib/curlx/winapi.c
    repo/lib/vauth/cleartext.c
    repo/lib/vauth/cram.c
    repo/lib/vauth/digest.c
    repo/lib/vauth/digest_sspi.c
    repo/lib/vauth/gsasl.c
    repo/lib/vauth/krb5_gssapi.c
    repo/lib/vauth/krb5_sspi.c
    repo/lib/vauth/ntlm.c
    repo/lib/vauth/ntlm_sspi.c
    repo/lib/vauth/oauth2.c
    repo/lib/vauth/spnego_gssapi.c
    repo/lib/vauth/spnego_sspi.c
    repo/lib/vauth/vauth.c
    repo/lib/vtls/apple.c
    repo/lib/vtls/cipher_suite.c
    repo/lib/vtls/gtls.c
    repo/lib/vtls/hostcheck.c
    repo/lib/vtls/keylog.c
    repo/lib/vtls/mbedtls.c
    repo/lib/vtls/openssl.c
    repo/lib/vtls/rustls.c
    repo/lib/vtls/schannel.c
    repo/lib/vtls/schannel_verify.c
    repo/lib/vtls/vtls.c
    repo/lib/vtls/vtls_scache.c
    repo/lib/vtls/vtls_spack.c
    repo/lib/vtls/wolfssl.c
    repo/lib/vtls/x509asn1.c
    repo/lib/vquic/curl_ngtcp2.c
    repo/lib/vquic/curl_quiche.c
    repo/lib/vquic/vquic.c
    repo/lib/vquic/vquic-tls.c
    repo/lib/vssh/libssh.c
    repo/lib/vssh/libssh2.c
    repo/lib/vssh/vssh.c
    repo/lib/altsvc.c
    repo/lib/amigaos.c
    repo/lib/asyn-ares.c
    repo/lib/asyn-base.c
    repo/lib/asyn-thrdd.c
    repo/lib/bufq.c
    repo/lib/bufref.c
    repo/lib/cf-h1-proxy.c
    repo/lib/cf-h2-proxy.c
    repo/lib/cf-haproxy.c
    repo/lib/cf-https-connect.c
    repo/lib/cf-ip-happy.c
    repo/lib/cf-socket.c
    repo/lib/cfilters.c
    repo/lib/conncache.c
    repo/lib/connect.c
    repo/lib/content_encoding.c
    repo/lib/cookie.c
    repo/lib/cshutdn.c
    repo/lib/curl_addrinfo.c
    repo/lib/curl_endian.c
    repo/lib/curl_fnmatch.c
    repo/lib/curl_fopen.c
    repo/lib/curl_get_line.c
    repo/lib/curl_gethostname.c
    repo/lib/curl_gssapi.c
    repo/lib/curl_memrchr.c
    repo/lib/curl_ntlm_core.c
    repo/lib/curl_range.c
    repo/lib/curl_rtmp.c
    repo/lib/curl_sasl.c
    repo/lib/curl_sha512_256.c
    repo/lib/curl_share.c
    repo/lib/curl_sspi.c
    repo/lib/curl_threads.c
    repo/lib/curl_trc.c
    repo/lib/cw-out.c
    repo/lib/cw-pause.c
    repo/lib/dict.c
    repo/lib/doh.c
    repo/lib/dynhds.c
    repo/lib/easy.c
    repo/lib/easygetopt.c
    repo/lib/easyoptions.c
    repo/lib/escape.c
    repo/lib/fake_addrinfo.c
    repo/lib/file.c
    repo/lib/fileinfo.c
    repo/lib/formdata.c
    repo/lib/ftp.c
    repo/lib/ftplistparser.c
    ${CURL_GEN}/lib/getenv.c
    repo/lib/getinfo.c
    repo/lib/gopher.c
    repo/lib/hash.c
    repo/lib/headers.c
    repo/lib/hmac.c
    repo/lib/hostip.c
    repo/lib/hostip4.c
    repo/lib/hostip6.c
    repo/lib/hsts.c
    repo/lib/http.c
    repo/lib/http1.c
    repo/lib/http2.c
    repo/lib/http_aws_sigv4.c
    repo/lib/http_chunks.c
    repo/lib/http_digest.c
    repo/lib/http_negotiate.c
    repo/lib/http_ntlm.c
    repo/lib/http_proxy.c
    repo/lib/httpsrr.c
    repo/lib/idn.c
    repo/lib/if2ip.c
    repo/lib/imap.c
    repo/lib/ldap.c
    repo/lib/llist.c
    repo/lib/macos.c
    repo/lib/md4.c
    repo/lib/md5.c
    repo/lib/memdebug.c
    repo/lib/mime.c
    repo/lib/mprintf.c
    repo/lib/mqtt.c
    repo/lib/multi.c
    repo/lib/multi_ev.c
    repo/lib/multi_ntfy.c
    repo/lib/netrc.c
    repo/lib/noproxy.c
    repo/lib/openldap.c
    repo/lib/parsedate.c
    repo/lib/pingpong.c
    repo/lib/pop3.c
    repo/lib/progress.c
    repo/lib/psl.c
    ${CURL_GEN}/lib/rand.c
    repo/lib/ratelimit.c
    repo/lib/request.c
    repo/lib/rtsp.c
    repo/lib/select.c
    repo/lib/sendf.c
    repo/lib/setopt.c
    repo/lib/sha256.c
    repo/lib/slist.c
    repo/lib/smb.c
    repo/lib/smtp.c
    repo/lib/socketpair.c
    repo/lib/socks.c
    repo/lib/socks_gssapi.c
    repo/lib/socks_sspi.c
    repo/lib/splay.c
    repo/lib/strcase.c
    repo/lib/strequal.c
    repo/lib/strerror.c
    repo/lib/system_win32.c
    repo/lib/telnet.c
    repo/lib/tftp.c
    repo/lib/transfer.c
    repo/lib/uint-bset.c
    repo/lib/uint-hash.c
    repo/lib/uint-spbset.c
    repo/lib/uint-table.c
    repo/lib/url.c
    repo/lib/urlapi.c
    repo/lib/version.c
    repo/lib/ws.c
    repo/lib/dllmain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/moxnox_fopen_compat.c
)

target_include_directories(libcurl PRIVATE
    repo/lib
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(libcurl PUBLIC repo/include)

target_compile_definitions(libcurl PRIVATE BUILDING_LIBCURL HAVE_CONFIG_H)

# force libcurl to use select in posixshim
target_compile_definitions(libcurl PRIVATE __INSIDE_CYGWIN__)
target_link_libraries(libcurl PRIVATE posixshim)

target_compile_options(libcurl PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-type-limits
    "SHELL:-include ${CMAKE_SOURCE_DIR}/include/sys/select.h"
    "SHELL:-include ${CURL_GEN}/lib/setup-win32.h"
    "SHELL:-include ${CURL_GEN}/lib/curlx/version_win32.h"
)

install(TARGETS libcurl DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
)

add_executable(curl_exe
    repo/src/config2setopts.c
    repo/src/slist_wc.c
    ${CURL_GEN}/src/terminal.c
    repo/src/tool_cb_dbg.c
    repo/src/tool_cb_hdr.c
    repo/src/tool_cb_prg.c
    repo/src/tool_cb_rea.c
    repo/src/tool_cb_see.c
    repo/src/tool_cb_soc.c
    ${CURL_GEN}/src/tool_cb_wrt.c
    repo/src/tool_cfgable.c
    repo/src/tool_dirhie.c
    ${CURL_GEN}/src/tool_doswin.c
    repo/src/tool_easysrc.c
    ${CURL_GEN}/src/tool_filetime.c
    repo/src/tool_findfile.c
    repo/src/tool_formparse.c
    repo/src/tool_getparam.c
    ${CURL_GEN}/src/tool_getpass.c
    repo/src/tool_help.c
    repo/src/tool_helpers.c
    repo/src/tool_ipfs.c
    repo/src/tool_libinfo.c
    repo/src/tool_listhelp.c
    repo/src/tool_main.c
    repo/src/tool_msgs.c
    ${CURL_GEN}/src/tool_operate.c
    repo/src/tool_operhlp.c
    repo/src/tool_paramhlp.c
    ${CURL_GEN}/src/tool_parsecfg.c
    repo/src/tool_progress.c
    repo/src/tool_setopt.c
    repo/src/tool_ssls.c
    repo/src/tool_stderr.c
    repo/src/tool_urlglob.c
    ${CURL_GEN}/src/tool_util.c
    repo/src/tool_writeout.c
    repo/src/tool_writeout_json.c
    repo/src/var.c
    repo/src/toolx/tool_time.c
    repo/lib/curlx/base64.c
    repo/lib/curlx/basename.c
    repo/lib/curlx/dynbuf.c
    ${CURL_GEN}/lib/curlx/fopen.c
    repo/lib/curlx/nonblock.c
    repo/lib/curlx/strcopy.c
    repo/lib/curlx/strdup.c
    repo/lib/curlx/strerr.c
    repo/lib/curlx/strparse.c
    repo/lib/curlx/timeval.c
    repo/lib/curlx/wait.c
    repo/lib/curlx/warnless.c
    repo/lib/curlx/winapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/moxnox_fopen_compat.c
)

target_include_directories(curl_exe PRIVATE
    repo/lib
    repo/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(curl_exe PRIVATE HAVE_CONFIG_H)

target_compile_options(curl_exe PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    "SHELL:-include ${CMAKE_SOURCE_DIR}/include/sys/select.h"
    "SHELL:-include ${CURL_GEN}/lib/setup-win32.h"
    "SHELL:-include ${CURL_GEN}/lib/curlx/version_win32.h"
)

set_target_properties(curl_exe PROPERTIES OUTPUT_NAME curl)

target_link_libraries(curl_exe PRIVATE libcurl posixshim moxnox)

install(TARGETS curl_exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
