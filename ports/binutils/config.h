/* Headers present */
#define HAVE_FCNTL_H          1
#define HAVE_INTTYPES_H       1
#define HAVE_LIMITS_H         1
#define HAVE_MALLOC_H         1
#define HAVE_SPAWN_H          1
#define HAVE_STDINT_H         1
#define HAVE_STDLIB_H         1
#define HAVE_STRING_H         1
#define HAVE_STRINGS_H        1
#define HAVE_SYS_STAT_H       1
#define HAVE_SYS_TIME_H       1
#define HAVE_SYS_TYPES_H      1
#define HAVE_SYS_WAIT_H       1
#define HAVE_TIME_H           1
#define HAVE_UNISTD_H         1

/* Functions provided by ucrtbase / mingw-w64 */
#define HAVE_ASPRINTF         1
#define HAVE_ATEXIT           1
#define HAVE_BSEARCH          1
#define HAVE_CALLOC           1
#define HAVE_CLOCK            1
#define HAVE_GETCWD           1
#define HAVE_GETTIMEOFDAY     1
#define HAVE_MEMCHR           1
#define HAVE_MEMCMP           1
#define HAVE_MEMCPY           1
#define HAVE_MEMMOVE          1
#define HAVE_MEMPCPY          1
#define HAVE_MEMSET           1
#define HAVE_RENAME           1
#define HAVE_SETENV           1
#define HAVE_SNPRINTF         1
#define HAVE_POSIX_SPAWN      1
#define HAVE_POSIX_SPAWNP     1
#define HAVE_STRCASECMP       1
#define HAVE_STRCHR           1
#define HAVE_STRDUP           1
#define HAVE_STRERROR         1
#define HAVE_STRNCASECMP      1
#define HAVE_STRNLEN          1
#define HAVE_STRRCHR          1
#define HAVE_STRSTR           1
#define HAVE_STRTOD           1
#define HAVE_STRTOL           1
#define HAVE_STRTOLL          1
#define HAVE_STRTOUL          1
#define HAVE_STRTOULL         1
#define HAVE_TMPNAM           1
#define HAVE_UTIMENSAT        1
#define HAVE_VASPRINTF        1
#define HAVE_VFPRINTF         1
#define HAVE_VPRINTF          1
#define HAVE_VSPRINTF         1

/* Declarations available */
#define HAVE_DECL_ASPRINTF    0
#define HAVE_DECL_CALLOC      1
#define HAVE_DECL_GETENV      1
#define HAVE_DECL_MALLOC      1
#define HAVE_DECL_REALLOC     1
#define HAVE_DECL_SNPRINTF    1
#define HAVE_DECL_STRNLEN     1
#define HAVE_DECL_STRTOL      1
#define HAVE_DECL_STRTOLL     1
#define HAVE_DECL_STRTOUL     1
#define HAVE_DECL_STRTOULL    1
#define HAVE_DECL_VASPRINTF   0
#define HAVE_DECL_VSNPRINTF   1

/* Absent declarations - defined to 0 because some code uses #if, not #ifdef */
#define HAVE_DECL_BASENAME    0
#define HAVE_DECL_FFS         0
#define HAVE_DECL_GETOPT      0
#define HAVE_DECL_SBRK        0
#define HAVE_DECL_STRVERSCMP  0

/* Type sizes: x86_64 Windows LLP64 */
#define SIZEOF_INT            4
#define SIZEOF_LONG           4
#define SIZEOF_LONG_LONG      8
#define SIZEOF_SIZE_T         8

/* Other */
#define HAVE_INTPTR_T         1
#define HAVE_LONG_LONG        1
#define HAVE_UINTPTR_T        1
#define STDC_HEADERS          1
#define STACK_DIRECTION       -1
#define UNSIGNED_64BIT_TYPE   unsigned long long

/* Package */
#define PACKAGE_NAME          "libiberty"
#define PACKAGE_VERSION       "2.44"
#define PACKAGE_STRING        "libiberty 2.44"
#define PACKAGE_TARNAME       "libiberty"
#define PACKAGE_BUGREPORT     ""
#define PACKAGE_URL           ""

/* bfd-specific */
#define USE_BINARY_FOPEN      1
#define USE_64_BIT_ARCHIVE    1
#define HAVE_FDOPEN           1
#define HAVE_FILENO           1
#define HAVE_FTELLO64         1
#define HAVE_FSEEKO64         1
#define SIZEOF_OFF_T          8
#define SIZEOF_VOID_P         8
#define HAVE_DECL_FSEEKO64    1
#define HAVE_DECL_FTELLO64    1

/* gas-specific */
#define I386COFF                          1
#define DEFAULT_SFRAME                    0
#define DEFAULT_X86_TLS_CHECK             1
#define DEFAULT_X86_USED_NOTE             0
#define DEFAULT_GENERATE_ELF_STT_COMMON   0
#define DEFAULT_GENERATE_BUILD_NOTES      0
#define DEFAULT_RISCV_ATTR                1
#define DEFAULT_COMPRESSED_DEBUG_ALGORITHM 0  /* COMPRESS_DEBUG_NONE */
#define EMULATIONS                        ""
#ifndef DEFAULT_EMULATION
#define DEFAULT_EMULATION                 ""
#endif
#define TARGET_ALIAS                      "x86_64-w64-mingw32"
#define TARGET_CPU                        "x86_64"
#define TARGET_CANONICAL                  "x86_64-w64-mingw32"
#define TARGET_OS                         "mingw32"
#define TARGET_VENDOR                     "w64"
#define HAVE_WINDOWS_H                    1
#define VERSION                           "2.44"
#define ENABLE_LEAK_CHECK                 0

/* ld-specific */
#define DEFAULT_LD_TEXTREL_CHECK          textrel_check_none
#define DEFAULT_LD_TEXTREL_CHECK_WARNING  0
#define DEFAULT_EMIT_SYSV_HASH            false
#define DEFAULT_EMIT_GNU_HASH             false
#define ELF_LIST_OPTIONS                  false
#define ELF_SHLIB_LIST_OPTIONS            false
#define ELF_PLT_UNWIND_LIST_OPTIONS       false
#define ELF_SFRAME_LIST_OPTIONS           false

/* binutils-specific */
#define HAVE_DECL_STPCPY      0
#define HAVE_DECL_ENVIRON     1
#define DEFAULT_FOR_FOLLOW_LINKS 1
#define DEFAULT_STRINGS_ALL      1
#define DEFAULT_AR_DETERMINISTIC 0
#define HAVE_GOOD_UTIME_H     0
#define HAVE_REALPATH         1
