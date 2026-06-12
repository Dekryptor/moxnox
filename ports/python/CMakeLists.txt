set(GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")

function(python_patch relpath)
    add_custom_command(
        OUTPUT ${GEN}/${relpath}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/${relpath} ${GEN}/${relpath}
        COMMAND sed -z -i ${ARGN} ${GEN}/${relpath}
        DEPENDS repo/${relpath}
        VERBATIM
    )
endfunction()

python_patch(Include/pythread.h
    -e [[s|#if (defined(__APPLE__) \|\| defined(__linux__) \|\| defined(_WIN32) \\|#if (defined(__APPLE__) \|\| defined(__linux__) \|\| (defined(_WIN32) \&\& !defined(MOXNOX)) \\|g]]
)

add_custom_command(
    OUTPUT ${GEN}/Include/Python.h
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/Include/Python.h ${GEN}/Include/Python.h
    DEPENDS repo/Include/Python.h
    VERBATIM
)

python_patch(Python/pytime.c
    -e [[s|#ifdef MS_WINDOWS\n// Convert PyTime_t to long.\n// Return 0 on success. Return -1 and clamp the value on overflow.|#if defined(MS_WINDOWS) \|\| defined(MOXNOX)\n// Convert PyTime_t to long.\n// Return 0 on success. Return -1 and clamp the value on overflow.|g]]
    -e [[s|#ifdef MS_WINDOWS\n    // On Windows, timeval.tv_sec type is long\n    res2 = _PyTime_AsCLong(tv_sec, &tv->tv_sec)\;|#if defined(MS_WINDOWS) \|\| defined(MOXNOX)\n    // On Windows, timeval.tv_sec type is long\n    res2 = _PyTime_AsCLong(tv_sec, \&tv->tv_sec)\;|g]]
)

python_patch(Modules/faulthandler.c
    -e [[s|static const size_t faulthandler_nsignals = \\\n    Py_ARRAY_LENGTH(faulthandler_handlers)\;|static const size_t faulthandler_nsignals =\n    sizeof(faulthandler_handlers) / sizeof(faulthandler_handlers[0])\;|g]]
)

python_patch(Modules/termios.c
    -e [[s|#include <sys/ioctl.h>\n||g]]
    -e [[s|#if defined(TIOCGWINSZ)\n    termiosmodulestate \*state = PyModule_GetState(module)\;|#if 1\n    termiosmodulestate *state = PyModule_GetState(module)\;|g]]
    -e [[s|    r = ioctl(fd, TIOCGWINSZ, &w)\;|    r = tcgetwinsize(fd, \&w)\;|g]]
    -e [[s|#if defined(TIOCGWINSZ) && defined(TIOCSWINSZ)|#if 1|g]]
    -e [[s|    if (ioctl(fd, TIOCGWINSZ, &w) == -1) {|    if (tcgetwinsize(fd, \&w) == -1) {|g]]
    -e [[s|    r = ioctl(fd, TIOCSWINSZ, &w)\;|    r = tcsetwinsize(fd, \&w)\;|g]]
)

python_patch(Modules/fcntlmodule.c
    -e [[s|#include <sys/ioctl.h>            // ioctl()\n||g]]
    -e [[s|#define IOCTL_BUFSZ 1024\n|#define IOCTL_BUFSZ 1024\n    PyErr_SetString(PyExc_OSError, "ioctl() is not supported on Moxnox")\;\n    return NULL\;\n#if 0\n|g]]
    -e [[s|    return PyLong_FromLong((long)ret)\;\n#undef IOCTL_BUFSZ|    return PyLong_FromLong((long)ret)\;\n#endif\n#undef IOCTL_BUFSZ|g]]
)

python_patch(Modules/expat/xmlparse.c
    -e [[s|#ifdef _WIN32\n#  define getpid GetCurrentProcessId|#if defined(_WIN32) \&\& !defined(MOXNOX)\n#  define getpid GetCurrentProcessId|g]]
)

python_patch(Modules/socketmodule.c
    -e [[s|    name = sock_decode_hostname(hbuf)\;|    name = PyUnicode_DecodeFSDefault(hbuf)\;|g]]
)

add_custom_target(python_patched_headers DEPENDS
    ${GEN}/Include/pythread.h
    ${GEN}/Include/Python.h
)

find_package(Python3 3.13...<3.14 COMPONENTS Interpreter REQUIRED)

file(STRINGS "repo/Lib/importlib/_bootstrap_external.py" _magic_line
    REGEX "^MAGIC_NUMBER = ")
string(REGEX MATCH "[0-9]+" PYTHON_EXPECTED_MAGIC "${_magic_line}")
if(NOT PYTHON_EXPECTED_MAGIC)
    message(FATAL_ERROR "python: could not parse MAGIC_NUMBER from the repo")
endif()

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c
        "import importlib.util as u; print(int.from_bytes(u.MAGIC_NUMBER[:2], 'little'))"
    OUTPUT_VARIABLE PYTHON_HOST_MAGIC
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _magic_res)
if(NOT _magic_res EQUAL 0)
    message(FATAL_ERROR "python: could not query host CPython bytecode magic number")
endif()

if(NOT PYTHON_HOST_MAGIC STREQUAL PYTHON_EXPECTED_MAGIC)
    message(FATAL_ERROR
        "python: host CPython bytecode magic ${PYTHON_HOST_MAGIC} (${Python3_EXECUTABLE}) "
        "does not match the repo's ${PYTHON_EXPECTED_MAGIC}; the frozen modules would be "
        "silently corrupt. Use a CPython 3.13 whose bytecode magic matches.")
endif()

set(PYTHON_FROZEN_NAMES
    importlib._bootstrap importlib._bootstrap_external zipimport
    abc codecs io
    _collections_abc _sitebuiltins genericpath ntpath posixpath os site stat
    importlib.util importlib.machinery runpy
    __hello__ __phello__ __phello__.ham __phello__.ham.eggs __phello__.spam
    frozen_only
    getpath
)
set(PYTHON_FROZEN_HEADERS)
foreach(_name ${PYTHON_FROZEN_NAMES})
    list(APPEND PYTHON_FROZEN_HEADERS ${GEN}/Python/frozen_modules/${_name}.h)
endforeach()

add_custom_command(
    OUTPUT ${PYTHON_FROZEN_HEADERS}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/freeze.py
            ${CMAKE_CURRENT_SOURCE_DIR}/repo ${GEN}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/freeze.py
            repo/Tools/build/freeze_modules.py
    VERBATIM
)
add_custom_target(python_frozen DEPENDS ${PYTHON_FROZEN_HEADERS})

set(PYTHON_CORE_SOURCES
    repo/Modules/_abc.c
    repo/Modules/atexitmodule.c
    repo/Modules/_codecsmodule.c
    repo/Modules/_collectionsmodule.c
    repo/Modules/errnomodule.c
    ${GEN}/Modules/faulthandler.c
    repo/Modules/_functoolsmodule.c
    repo/Modules/gcmodule.c
    repo/Modules/getbuildinfo.c
    repo/Modules/_io/bufferedio.c
    repo/Modules/_io/bytesio.c
    repo/Modules/_io/fileio.c
    repo/Modules/_io/iobase.c
    repo/Modules/_io/_iomodule.c
    repo/Modules/_io/stringio.c
    repo/Modules/_io/textio.c
    repo/Modules/arraymodule.c
    repo/Modules/binascii.c
    repo/Modules/_bisectmodule.c
    repo/Modules/cmathmodule.c
    repo/Modules/_contextvarsmodule.c
    repo/Modules/_csv.c
    repo/Modules/_datetimemodule.c
    repo/Modules/_heapqmodule.c
    repo/Modules/_json.c
    repo/Modules/mathmodule.c
    repo/Modules/mmapmodule.c
    repo/Modules/_pickle.c
    repo/Modules/_queuemodule.c
    repo/Modules/_randommodule.c
    repo/Modules/_statisticsmodule.c
    repo/Modules/_zoneinfo.c
    repo/Modules/zlibmodule.c
    repo/Modules/readline.c
    repo/Modules/cjkcodecs/multibytecodec.c
    repo/Modules/cjkcodecs/_codecs_cn.c
    repo/Modules/cjkcodecs/_codecs_hk.c
    repo/Modules/cjkcodecs/_codecs_iso2022.c
    repo/Modules/cjkcodecs/_codecs_jp.c
    repo/Modules/cjkcodecs/_codecs_kr.c
    repo/Modules/cjkcodecs/_codecs_tw.c
    repo/Modules/md5module.c
    repo/Modules/sha1module.c
    repo/Modules/sha2module.c
    repo/Modules/sha3module.c
    repo/Modules/_hacl/Hacl_Hash_MD5.c
    repo/Modules/_hacl/Hacl_Hash_SHA1.c
    repo/Modules/_hacl/Hacl_Hash_SHA2.c
    repo/Modules/_hacl/Hacl_Hash_SHA3.c
    repo/Modules/_blake2/blake2module.c
    repo/Modules/_blake2/blake2b_impl.c
    repo/Modules/_blake2/blake2s_impl.c
    repo/Modules/pyexpat.c
    repo/Modules/_elementtree.c
    ${GEN}/Modules/expat/xmlparse.c
    repo/Modules/expat/xmlrole.c
    repo/Modules/expat/xmltok.c
    repo/Modules/_cursesmodule.c
    ${GEN}/Modules/fcntlmodule.c
    repo/Modules/itertoolsmodule.c
    repo/Modules/_localemodule.c
    repo/Modules/main.c
    repo/Modules/_opcode.c
    repo/Modules/_operator.c
    repo/Modules/posixmodule.c
    repo/Modules/pwdmodule.c
    repo/Modules/selectmodule.c
    ${GEN}/Modules/socketmodule.c
    repo/Modules/_ssl.c
    repo/Modules/_hashopenssl.c
    repo/Modules/signalmodule.c
    repo/Modules/_sre/sre.c
    repo/Modules/_stat.c
    repo/Modules/_struct.c
    repo/Modules/_suggestions.c
    repo/Modules/symtablemodule.c
    repo/Modules/_threadmodule.c
    ${GEN}/Modules/termios.c
    repo/Modules/timemodule.c
    repo/Modules/_tracemalloc.c
    repo/Modules/_typingmodule.c
    repo/Modules/unicodedata.c
    repo/Modules/_weakref.c
    repo/Objects/abstract.c
    repo/Objects/boolobject.c
    repo/Objects/bytearrayobject.c
    repo/Objects/bytes_methods.c
    repo/Objects/bytesobject.c
    repo/Objects/call.c
    repo/Objects/capsule.c
    repo/Objects/cellobject.c
    repo/Objects/classobject.c
    repo/Objects/codeobject.c
    repo/Objects/complexobject.c
    repo/Objects/descrobject.c
    repo/Objects/dictobject.c
    repo/Objects/enumobject.c
    repo/Objects/exceptions.c
    repo/Objects/fileobject.c
    repo/Objects/floatobject.c
    repo/Objects/frameobject.c
    repo/Objects/funcobject.c
    repo/Objects/genericaliasobject.c
    repo/Objects/genobject.c
    repo/Objects/iterobject.c
    repo/Objects/listobject.c
    repo/Objects/longobject.c
    repo/Objects/memoryobject.c
    repo/Objects/methodobject.c
    repo/Objects/moduleobject.c
    repo/Objects/namespaceobject.c
    repo/Objects/object.c
    repo/Objects/obmalloc.c
    repo/Objects/odictobject.c
    repo/Objects/picklebufobject.c
    repo/Objects/rangeobject.c
    repo/Objects/setobject.c
    repo/Objects/sliceobject.c
    repo/Objects/structseq.c
    repo/Objects/tupleobject.c
    repo/Objects/typeobject.c
    repo/Objects/typevarobject.c
    repo/Objects/unicodectype.c
    repo/Objects/unicodeobject.c
    repo/Objects/unionobject.c
    repo/Objects/weakrefobject.c
    repo/Parser/action_helpers.c
    repo/Parser/lexer/buffer.c
    repo/Parser/lexer/lexer.c
    repo/Parser/lexer/state.c
    repo/Parser/myreadline.c
    repo/Parser/parser.c
    repo/Parser/peg_api.c
    repo/Parser/pegen_errors.c
    repo/Parser/pegen.c
    repo/Parser/string_parser.c
    repo/Parser/tokenizer/file_tokenizer.c
    repo/Parser/tokenizer/helpers.c
    repo/Parser/tokenizer/readline_tokenizer.c
    repo/Parser/tokenizer/string_tokenizer.c
    repo/Parser/tokenizer/utf8_tokenizer.c
    repo/Parser/token.c
    repo/Python/asdl.c
    repo/Python/assemble.c
    repo/Python/ast.c
    repo/Python/ast_opt.c
    repo/Python/ast_unparse.c
    repo/Python/bltinmodule.c
    repo/Python/bootstrap_hash.c
    repo/Python/brc.c
    repo/Python/ceval_gil.c
    repo/Python/ceval.c
    repo/Python/codecs.c
    repo/Python/compile.c
    repo/Python/context.c
    repo/Python/critical_section.c
    repo/Python/crossinterp.c
    repo/Python/dtoa.c
    repo/Python/dynamic_annotations.c
    repo/Python/dynload_stub.c
    repo/Python/errors.c
    repo/Python/fileutils.c
    repo/Python/flowgraph.c
    repo/Python/formatter_unicode.c
    repo/Python/frame.c
    repo/Python/frozenmain.c
    repo/Python/future.c
    repo/Python/gc_free_threading.c
    repo/Python/gc_gil.c
    repo/Python/gc.c
    repo/Python/getargs.c
    repo/Python/getcompiler.c
    repo/Python/getcopyright.c
    repo/Python/getopt.c
    repo/Python/getplatform.c
    repo/Python/getversion.c
    repo/Python/hamt.c
    repo/Python/hashtable.c
    repo/Python/importdl.c
    repo/Python/import.c
    repo/Python/instruction_sequence.c
    repo/Python/instrumentation.c
    repo/Python/intrinsics.c
    repo/Python/jit.c
    repo/Python/legacy_tracing.c
    repo/Python/lock.c
    repo/Python/marshal.c
    repo/Python/modsupport.c
    repo/Python/mysnprintf.c
    repo/Python/mystrtoul.c
    repo/Python/object_stack.c
    repo/Python/optimizer_analysis.c
    repo/Python/optimizer.c
    repo/Python/optimizer_symbols.c
    repo/Python/parking_lot.c
    repo/Python/pyarena.c
    repo/Python/pyctype.c
    repo/Python/pyfpe.c
    repo/Python/pyhash.c
    repo/Python/pylifecycle.c
    repo/Python/pymath.c
    repo/Python/pystate.c
    repo/Python/pystrcmp.c
    repo/Python/pystrhex.c
    repo/Python/pystrtod.c
    repo/Python/Python-ast.c
    repo/Python/pythonrun.c
    repo/Python/Python-tokenize.c
    ${GEN}/Python/pytime.c
    repo/Python/qsbr.c
    repo/Python/specialize.c
    repo/Python/structmember.c
    repo/Python/suggestions.c
    repo/Python/symtable.c
    repo/Python/sysmodule.c
    repo/Python/thread.c
    repo/Python/traceback.c
    repo/Python/tracemalloc.c
    repo/Python/_warnings.c
    repo/Python/initconfig.c
    repo/Python/interpconfig.c
    repo/Python/pathconfig.c
    repo/Python/perf_trampoline.c
    repo/Python/perf_jit_trampoline.c
    repo/Python/preconfig.c
    repo/Modules/_sysconfig.c
    repo/Python/frozen.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(PYTHON_MPD_ASM "ANSI")
else()
    set(PYTHON_MPD_ASM "ASM")
endif()
set(PYTHON_DECIMAL_SOURCES
    repo/Modules/_decimal/_decimal.c
    repo/Modules/_decimal/libmpdec/basearith.c
    repo/Modules/_decimal/libmpdec/constants.c
    repo/Modules/_decimal/libmpdec/context.c
    repo/Modules/_decimal/libmpdec/convolute.c
    repo/Modules/_decimal/libmpdec/crt.c
    repo/Modules/_decimal/libmpdec/difradix2.c
    repo/Modules/_decimal/libmpdec/fnt.c
    repo/Modules/_decimal/libmpdec/fourstep.c
    repo/Modules/_decimal/libmpdec/io.c
    repo/Modules/_decimal/libmpdec/mpalloc.c
    repo/Modules/_decimal/libmpdec/mpdecimal.c
    repo/Modules/_decimal/libmpdec/mpsignal.c
    repo/Modules/_decimal/libmpdec/numbertheory.c
    repo/Modules/_decimal/libmpdec/sixstep.c
    repo/Modules/_decimal/libmpdec/transpose.c
)
set_source_files_properties(${PYTHON_DECIMAL_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "CONFIG_64;${PYTHON_MPD_ASM}"
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules/_decimal/libmpdec")

add_library(libpython STATIC
    ${PYTHON_CORE_SOURCES}
    ${PYTHON_DECIMAL_SOURCES}
    config.c
    repo/Modules/getpath.c
)

add_dependencies(libpython python_frozen python_patched_headers)

set_source_files_properties(repo/Modules/getpath.c PROPERTIES COMPILE_DEFINITIONS
    "PYTHONPATH=\"\";PREFIX=\"/\";EXEC_PREFIX=\"/\";VERSION=\"3.13\";VPATH=\"\";PLATLIBDIR=\"lib\";PYTHONFRAMEWORK=\"\"")

set_source_files_properties(repo/Modules/selectmodule.c PROPERTIES
    COMPILE_OPTIONS "-include;sys/select.h")

set_source_files_properties(${GEN}/Modules/termios.c PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules")

set_source_files_properties(${GEN}/Modules/fcntlmodule.c PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules")

set_source_files_properties(${GEN}/Modules/socketmodule.c PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules")

set_source_files_properties(repo/Modules/_ssl.c PROPERTIES
    COMPILE_DEFINITIONS "OPENSSL_NO_TLS1_METHOD;OPENSSL_NO_TLS1_1_METHOD;OPENSSL_NO_TLS1_2_METHOD")

target_include_directories(libpython PRIVATE
    repo/Include/internal
    ${GEN}
    ${GEN}/Python
    repo
    repo/Modules/_hacl/include
)

set_source_files_properties(
    repo/Modules/_blake2/blake2module.c
    repo/Modules/_blake2/blake2b_impl.c
    repo/Modules/_blake2/blake2s_impl.c
    PROPERTIES COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules/_blake2/impl")

set_source_files_properties(
    repo/Modules/pyexpat.c
    repo/Modules/_elementtree.c
    ${GEN}/Modules/expat/xmlparse.c
    repo/Modules/expat/xmlrole.c
    repo/Modules/expat/xmltok.c
    PROPERTIES
        COMPILE_DEFINITIONS "XML_STATIC"
        COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules/expat")

set_source_files_properties(repo/Modules/mmapmodule.c PROPERTIES
    COMPILE_DEFINITIONS "my_getallocationgranularity=my_getpagesize")

target_include_directories(libpython PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/Include
    repo/Include
)

target_compile_definitions(libpython PRIVATE
    Py_BUILD_CORE
    PLATFORM="moxnox"
)

target_compile_options(libpython PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-missing-field-initializers
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-type-limits
    -Wno-sign-compare
    -Wno-format
)

target_link_libraries(libpython PRIVATE moxnox ncurses zlib readline libssl libcrypto)

add_executable(python_exe repo/Programs/python.c)

target_compile_definitions(python_exe PRIVATE Py_BUILD_CORE)

target_compile_options(python_exe PRIVATE ${USERSPACE_CFLAGS})

set_target_properties(python_exe PROPERTIES OUTPUT_NAME python)

target_link_libraries(python_exe PRIVATE libpython moxnox)

add_dependencies(python_exe python_patched_headers)

install(TARGETS python_exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(DIRECTORY repo/Lib/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13
    PATTERN "__pycache__" EXCLUDE
    PATTERN "test" EXCLUDE
    PATTERN "tests" EXCLUDE)

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13/lib-dynload)
