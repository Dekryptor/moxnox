/* pyconfig.h.  Generated from pyconfig.h.in by configure.  */
/* pyconfig.h.in.  Generated from configure.ac by autoheader.  */


#ifndef Py_PYCONFIG_H
#define Py_PYCONFIG_H


/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* BUILD_GNU_TYPE + AIX_BUILDDATE are used to construct the PEP425 tag of the
   build system. */
/* #undef AIX_BUILDDATE */

/* Define for AIX if your compiler is a genuine IBM xlC/xlC_r and you want
   support for AIX C++ shared extension modules. */
/* #undef AIX_GENUINE_CPLUSPLUS */

/* The normal alignment of `long', in bytes. */
#define ALIGNOF_LONG 8

/* The normal alignment of `max_align_t', in bytes. */
#define ALIGNOF_MAX_ALIGN_T 16

/* The normal alignment of `size_t', in bytes. */
#define ALIGNOF_SIZE_T 8

/* Alternative SOABI used in debug build to load C extensions built in release
   mode */
/* #undef ALT_SOABI */

/* The Android API level. */
/* #undef ANDROID_API_LEVEL */

/* Define if C doubles are 64-bit IEEE 754 binary format, stored in ARM
   mixed-endian order (byte order 45670123) */
/* #undef DOUBLE_IS_ARM_MIXED_ENDIAN_IEEE754 */

/* Define if C doubles are 64-bit IEEE 754 binary format, stored with the most
   significant byte first */
/* #undef DOUBLE_IS_BIG_ENDIAN_IEEE754 */

/* Define if C doubles are 64-bit IEEE 754 binary format, stored with the
   least significant byte first */
#define DOUBLE_IS_LITTLE_ENDIAN_IEEE754 1

/* Define if --enable-ipv6 is specified */
#define ENABLE_IPV6 1

/* Define if getpgrp() must be called as getpgrp(0). */
/* #undef GETPGRP_HAVE_ARG */

/* Define if you have the 'accept' function. */
#define HAVE_ACCEPT 1

/* Define to 1 if you have the `accept4' function. */
#define HAVE_ACCEPT4 1

/* Define to 1 if you have the `acosh' function. */
#define HAVE_ACOSH 1

/* struct addrinfo (netdb.h) */
#define HAVE_ADDRINFO 1

/* Define to 1 if you have the `alarm' function. */
#define HAVE_ALARM 1

/* Define if aligned memory access is required */
/* #undef HAVE_ALIGNED_REQUIRED */

/* Define to 1 if you have the <alloca.h> header file. */
#define HAVE_ALLOCA_H 1

/* Define this if your time.h defines altzone. */
/* #undef HAVE_ALTZONE */

/* Define to 1 if you have the `asinh' function. */
#define HAVE_ASINH 1

/* Define to 1 if you have the <asm/types.h> header file. */
#define HAVE_ASM_TYPES_H 1

/* Define to 1 if you have the `atanh' function. */
#define HAVE_ATANH 1

/* Define if you have the 'bind' function. */
#define HAVE_BIND 1

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
/* #undef HAVE_BIND_TEXTDOMAIN_CODESET (moxnox) */

/* Define to 1 if you have the <bluetooth/bluetooth.h> header file. */
/* #undef HAVE_BLUETOOTH_BLUETOOTH_H (moxnox) */

/* Define to 1 if you have the <bluetooth.h> header file. */
/* #undef HAVE_BLUETOOTH_H */

/* Define if mbstowcs(NULL, "text", 0) does not return the number of wide
   chars that would be converted. */
/* #undef HAVE_BROKEN_MBSTOWCS */

/* Define if nice() returns success/failure instead of the new priority. */
/* #undef HAVE_BROKEN_NICE */

/* Define if the system reports an invalid PIPE_BUF value. */
/* #undef HAVE_BROKEN_PIPE_BUF */

/* Define if poll() sets errno on invalid file descriptors. */
/* #undef HAVE_BROKEN_POLL */

/* Define if the Posix semaphores do not work on your system */
/* #undef HAVE_BROKEN_POSIX_SEMAPHORES */

/* Define if pthread_sigmask() does not work on your system. */
/* #undef HAVE_BROKEN_PTHREAD_SIGMASK */

/* define to 1 if your sem_getvalue is broken. */
/* #undef HAVE_BROKEN_SEM_GETVALUE */

/* Define if 'unsetenv' does not return an int. */
/* #undef HAVE_BROKEN_UNSETENV */

/* Has builtin __atomic_load_n() and __atomic_store_n() functions */
/* #undef HAVE_BUILTIN_ATOMIC (moxnox) */

/* Define to 1 if you have the <bzlib.h> header file. */
#define HAVE_BZLIB_H 1

/* Define to 1 if you have the 'chflags' function. */
/* #undef HAVE_CHFLAGS */

/* Define to 1 if you have the `chmod' function. */
#define HAVE_CHMOD 1

/* Define to 1 if you have the `chown' function. */
/* #undef HAVE_CHOWN (moxnox) */

/* Define if you have the 'chroot' function. */
/* #undef HAVE_CHROOT (moxnox) */

/* Define to 1 if you have the `clock' function. */
#define HAVE_CLOCK 1

/* Define to 1 if you have the `clock_getres' function. */
/* #undef HAVE_CLOCK_GETRES (moxnox) */

/* Define to 1 if you have the `clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* Use UTF-8 unconditionally, ignoring the locale (moxnox) */
#define _Py_FORCE_UTF8_LOCALE 1

/* Define to 1 if you have the `clock_nanosleep' function. */
/* #undef HAVE_CLOCK_NANOSLEEP (moxnox) */

/* Define to 1 if you have the `clock_settime' function. */
/* #undef HAVE_CLOCK_SETTIME (moxnox) */

/* Define to 1 if the system has the type `clock_t'. */
#define HAVE_CLOCK_T 1

/* Define to 1 if you have the `closefrom' function. */
/* #undef HAVE_CLOSEFROM (moxnox) */

/* Define to 1 if you have the `close_range' function. */
/* #undef HAVE_CLOSE_RANGE (moxnox) */

/* Define if the C compiler supports computed gotos. */
#define HAVE_COMPUTED_GOTOS 1

/* Define to 1 if you have the `confstr' function. */
/* #undef HAVE_CONFSTR (moxnox) */

/* Define to 1 if you have the <conio.h> header file. */
/* #undef HAVE_CONIO_H */

/* Define if you have the 'connect' function. */
#define HAVE_CONNECT 1

/* Define to 1 if you have the `copy_file_range' function. */
/* #undef HAVE_COPY_FILE_RANGE (moxnox) */

/* Define to 1 if you have the `ctermid' function. */
/* #undef HAVE_CTERMID (moxnox) */

/* Define if you have the 'ctermid_r' function. */
/* #undef HAVE_CTERMID_R */

/* Define if you have the 'filter' function. */
#define HAVE_CURSES_FILTER 1

/* Define to 1 if you have the <curses.h> header file. */
#define HAVE_CURSES_H 1

/* Define if you have the 'has_key' function. */
#define HAVE_CURSES_HAS_KEY 1

/* Define if you have the 'immedok' function. */
#define HAVE_CURSES_IMMEDOK 1

/* Define if you have the 'is_pad' function. */
#define HAVE_CURSES_IS_PAD 1

/* Define if you have the 'is_term_resized' function. */
#define HAVE_CURSES_IS_TERM_RESIZED 1

/* Define if you have the 'resizeterm' function. */
#define HAVE_CURSES_RESIZETERM 1

/* Define if you have the 'resize_term' function. */
#define HAVE_CURSES_RESIZE_TERM 1

/* Define if you have the 'syncok' function. */
#define HAVE_CURSES_SYNCOK 1

/* Define if you have the 'typeahead' function. */
#define HAVE_CURSES_TYPEAHEAD 1

/* Define if you have the 'use_env' function. */
#define HAVE_CURSES_USE_ENV 1

/* Define if you have the 'wchgat' function. */
#define HAVE_CURSES_WCHGAT 1

/* Define to 1 if you have the <db.h> header file. */
/* #undef HAVE_DB_H */

/* Define to 1 if you have the declaration of `RTLD_DEEPBIND', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_DEEPBIND (moxnox) */

/* Define to 1 if you have the declaration of `RTLD_GLOBAL', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_GLOBAL (moxnox) */

/* Define to 1 if you have the declaration of `RTLD_LAZY', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_LAZY (moxnox) */

/* Define to 1 if you have the declaration of `RTLD_LOCAL', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_LOCAL (moxnox) */

/* Define to 1 if you have the declaration of `RTLD_MEMBER', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_MEMBER (moxnox) */

/* Define to 1 if you have the declaration of `RTLD_NODELETE', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_NODELETE (moxnox) */

/* Define to 1 if you have the declaration of `RTLD_NOLOAD', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_NOLOAD (moxnox) */

/* Define to 1 if you have the declaration of `RTLD_NOW', and to 0 if you
   don't. */
/* #undef HAVE_DECL_RTLD_NOW (moxnox) */

/* Define to 1 if you have the declaration of `tzname', and to 0 if you don't.
   */
/* #undef HAVE_DECL_TZNAME */

/* Define to 1 if you have the declaration of `UT_NAMESIZE', and to 0 if you
   don't. */
#define HAVE_DECL_UT_NAMESIZE 1

/* Define to 1 if you have the device macros. */
/* #undef HAVE_DEVICE_MACROS (moxnox) */

/* Define to 1 if you have the /dev/ptc device file. */
/* #undef HAVE_DEV_PTC */

/* Define to 1 if you have the /dev/ptmx device file. */
/* #undef HAVE_DEV_PTMX (moxnox) */

/* Define to 1 if you have the <direct.h> header file. */
/* #undef HAVE_DIRECT_H */

/* Define to 1 if the dirent structure has a d_type field */
#define HAVE_DIRENT_D_TYPE 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define if you have the 'dirfd' function or macro. */
/* #undef HAVE_DIRFD (moxnox) */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H (moxnox) */

/* Define to 1 if you have the `dlopen' function. */
/* #undef HAVE_DLOPEN (moxnox) */

/* Define to 1 if you have the `dup' function. */
#define HAVE_DUP 1

/* Define to 1 if you have the `dup2' function. */
#define HAVE_DUP2 1

/* Define to 1 if you have the `dup3' function. */
/* #undef HAVE_DUP3 (moxnox) */

/* Define if you have the '_dyld_shared_cache_contains_path' function. */
/* #undef HAVE_DYLD_SHARED_CACHE_CONTAINS_PATH */

/* Defined when any dynamic module loading is enabled. */
/* #undef HAVE_DYNAMIC_LOADING (moxnox) */

/* Define to 1 if you have the <editline/readline.h> header file. */
/* #undef HAVE_EDITLINE_READLINE_H */

/* Define to 1 if you have the <endian.h> header file. */
#define HAVE_ENDIAN_H 1

/* Define if you have the 'epoll_create' function. */
/* #undef HAVE_EPOLL (moxnox) */

/* Define if you have the 'epoll_create1' function. */
/* #undef HAVE_EPOLL_CREATE1 (moxnox) */

/* Define to 1 if you have the `erf' function. */
#define HAVE_ERF 1

/* Define to 1 if you have the `erfc' function. */
#define HAVE_ERFC 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define if you have the 'eventfd' function. */
/* #undef HAVE_EVENTFD (moxnox) */

/* Define to 1 if you have the `execv' function. */
/* #undef HAVE_EXECV (moxnox) */

/* Define to 1 if you have the `explicit_bzero' function. */
/* #undef HAVE_EXPLICIT_BZERO (moxnox) */

/* Define to 1 if you have the `explicit_memset' function. */
/* #undef HAVE_EXPLICIT_MEMSET */

/* Define to 1 if you have the `expm1' function. */
#define HAVE_EXPM1 1

/* Define to 1 if you have the `faccessat' function. */
/* #undef HAVE_FACCESSAT (moxnox) */

/* Define if you have the 'fchdir' function. */
/* #undef HAVE_FCHDIR (moxnox) */

/* Define to 1 if you have the `fchmod' function. */
/* #undef HAVE_FCHMOD (moxnox) */

/* Define to 1 if you have the `fchmodat' function. */
/* #undef HAVE_FCHMODAT (moxnox) */

/* Define to 1 if you have the `fchown' function. */
/* #undef HAVE_FCHOWN (moxnox) */

/* Define to 1 if you have the `fchownat' function. */
/* #undef HAVE_FCHOWNAT (moxnox) */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define if you have the 'fdatasync' function. */
/* #undef HAVE_FDATASYNC (moxnox) */

/* Define to 1 if you have the `fdopendir' function. */
/* #undef HAVE_FDOPENDIR (moxnox) */

/* Define to 1 if you have the `fdwalk' function. */
/* #undef HAVE_FDWALK */

/* Define to 1 if you have the `fexecve' function. */
/* #undef HAVE_FEXECVE (moxnox) */

/* Define if you have the 'ffi_closure_alloc' function. */
#define HAVE_FFI_CLOSURE_ALLOC 1

/* Define if you have the 'ffi_prep_cif_var' function. */
#define HAVE_FFI_PREP_CIF_VAR 1

/* Define if you have the 'ffi_prep_closure_loc' function. */
#define HAVE_FFI_PREP_CLOSURE_LOC 1

/* Define to 1 if you have the `flock' function. */
/* #undef HAVE_FLOCK (moxnox) */

/* Define to 1 if you have the `fork' function. */
/* #undef HAVE_FORK (moxnox) */

/* Define to 1 if you have the `fork1' function. */
/* #undef HAVE_FORK1 */

/* Define to 1 if you have the `forkpty' function. */
/* #undef HAVE_FORKPTY (moxnox) */

/* Define to 1 if you have the `fpathconf' function. */
/* #undef HAVE_FPATHCONF (moxnox) */

/* Define to 1 if you have the `fseek64' function. */
/* #undef HAVE_FSEEK64 */

/* Define to 1 if you have the `fseeko' function. */
/* #undef HAVE_FSEEKO (moxnox) */

/* Define to 1 if you have the `fstatat' function. */
/* #undef HAVE_FSTATAT (moxnox) */

/* Define to 1 if you have the `fstatvfs' function. */
/* #undef HAVE_FSTATVFS (moxnox) */

/* Define if you have the 'fsync' function. */
#define HAVE_FSYNC 1

/* Define to 1 if you have the `ftell64' function. */
/* #undef HAVE_FTELL64 */

/* Define to 1 if you have the `ftello' function. */
/* #undef HAVE_FTELLO (moxnox) */

/* Define to 1 if you have the `ftime' function. */
/* #undef HAVE_FTIME (moxnox) */

/* Define to 1 if you have the `ftruncate' function. */
#define HAVE_FTRUNCATE 1

/* Define to 1 if you have the `futimens' function. */
/* #undef HAVE_FUTIMENS (moxnox) */

/* Define to 1 if you have the `futimes' function. */
/* #undef HAVE_FUTIMES (moxnox) */

/* Define to 1 if you have the `futimesat' function. */
/* #undef HAVE_FUTIMESAT (moxnox) */

/* Define to 1 if you have the `gai_strerror' function. */
#define HAVE_GAI_STRERROR 1

/* Define if we can use gcc inline assembler to get and set mc68881 fpcr */
/* #undef HAVE_GCC_ASM_FOR_MC68881 */

/* Define if we can use x64 gcc inline assembler */
#ifdef __x86_64__
#define HAVE_GCC_ASM_FOR_X64 1
#endif

/* Define if we can use gcc inline assembler to get and set x87 control word
   */
#ifdef __x86_64__
#define HAVE_GCC_ASM_FOR_X87 1
#endif

/* Define if your compiler provides __uint128_t */
#define HAVE_GCC_UINT128_T 1

/* Define to 1 if you have the <gdbm-ndbm.h> header file. */
/* #undef HAVE_GDBM_DASH_NDBM_H */

/* Define to 1 if you have the <gdbm.h> header file. */
/* #undef HAVE_GDBM_H (moxnox) */

/* Define to 1 if you have the <gdbm/ndbm.h> header file. */
/* #undef HAVE_GDBM_NDBM_H (moxnox) */

/* Define if you have the getaddrinfo function. */
#define HAVE_GETADDRINFO 1

/* Define this if you have flockfile(), getc_unlocked(), and funlockfile() */
/* #undef HAVE_GETC_UNLOCKED (moxnox) */

/* Define to 1 if you have the `getegid' function. */
/* #undef HAVE_GETEGID (moxnox) */

/* Define to 1 if you have the `getentropy' function. */
#define HAVE_GETENTROPY 1

/* Define to 1 if you have the `geteuid' function. */
/* #undef HAVE_GETEUID (moxnox) */

/* Define to 1 if you have the `getgid' function. */
#define HAVE_GETGID 1

/* Define to 1 if you have the `getgrent' function. */
#define HAVE_GETGRENT 1

/* Define to 1 if you have the `getgrgid' function. */
/* #undef HAVE_GETGRGID (moxnox) */

/* Define to 1 if you have the `getgrgid_r' function. */
/* #undef HAVE_GETGRGID_R (moxnox) */

/* Define to 1 if you have the `getgrnam_r' function. */
/* #undef HAVE_GETGRNAM_R (moxnox) */

/* Define to 1 if you have the `getgrouplist' function. */
/* #undef HAVE_GETGROUPLIST (moxnox) */

/* Define to 1 if you have the `getgroups' function. */
/* #undef HAVE_GETGROUPS (moxnox) */

/* Define if you have the 'gethostbyaddr' function. */
/* #undef HAVE_GETHOSTBYADDR (moxnox) */

/* Define to 1 if you have the `gethostbyname' function. */
/* #undef HAVE_GETHOSTBYNAME (moxnox: use getaddrinfo) */

/* Define this if you have some version of gethostbyname_r() */
/* #undef HAVE_GETHOSTBYNAME_R (moxnox) */

/* Define this if you have the 3-arg version of gethostbyname_r(). */
/* #undef HAVE_GETHOSTBYNAME_R_3_ARG */

/* Define this if you have the 5-arg version of gethostbyname_r(). */
/* #undef HAVE_GETHOSTBYNAME_R_5_ARG */

/* Define this if you have the 6-arg version of gethostbyname_r(). */
/* #undef HAVE_GETHOSTBYNAME_R_6_ARG (moxnox) */

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getitimer' function. */
/* #undef HAVE_GETITIMER (moxnox) */

/* Define to 1 if you have the `getloadavg' function. */
/* #undef HAVE_GETLOADAVG (moxnox) */

/* Define to 1 if you have the `getlogin' function. */
/* #undef HAVE_GETLOGIN (moxnox) */

/* Define to 1 if you have the `getlogin_r' function. */
#define HAVE_GETLOGIN_R 1

/* Define to 1 if you have the `getnameinfo' function. */
#define HAVE_GETNAMEINFO 1

/* Define if you have the 'getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define if you have the 'getpeername' function. */
/* #undef HAVE_GETPEERNAME (moxnox) */

/* Define to 1 if you have the `getpgid' function. */
/* #undef HAVE_GETPGID (moxnox) */

/* Define to 1 if you have the `getpgrp' function. */
/* #undef HAVE_GETPGRP (moxnox) */

/* Define to 1 if you have the `getpid' function. */
#define HAVE_GETPID 1

/* Define to 1 if you have the `getppid' function. */
/* #undef HAVE_GETPPID (moxnox) */

/* Define to 1 if you have the `getpriority' function. */
/* #undef HAVE_GETPRIORITY (moxnox) */

/* Define if you have the 'getprotobyname' function. */
/* #undef HAVE_GETPROTOBYNAME (moxnox) */

/* Define to 1 if you have the `getpwent' function. */
/* #undef HAVE_GETPWENT (moxnox) */

/* Define to 1 if you have the `getpwnam_r' function. */
/* #undef HAVE_GETPWNAM_R (moxnox) */

/* Define to 1 if you have the `getpwuid' function. */
#define HAVE_GETPWUID 1

/* Define to 1 if you have the `getpwuid_r' function. */
/* #undef HAVE_GETPWUID_R (moxnox) */

/* Define to 1 if the getrandom() function is available */
/* #undef HAVE_GETRANDOM (moxnox) */

/* Define to 1 if the Linux getrandom() syscall is available */
/* #undef HAVE_GETRANDOM_SYSCALL (moxnox) */

/* Define to 1 if you have the `getresgid' function. */
/* #undef HAVE_GETRESGID (moxnox) */

/* Define to 1 if you have the `getresuid' function. */
/* #undef HAVE_GETRESUID (moxnox) */

/* Define to 1 if you have the `getrusage' function. */
#define HAVE_GETRUSAGE 1

/* Define if you have the 'getservbyname' function. */
/* #undef HAVE_GETSERVBYNAME (moxnox: use getaddrinfo) */

/* Define if you have the 'getservbyport' function. */
/* #undef HAVE_GETSERVBYPORT (moxnox: use getaddrinfo) */

/* Define to 1 if you have the `getsid' function. */
/* #undef HAVE_GETSID (moxnox) */

/* Define if you have the 'getsockname' function. */
#define HAVE_GETSOCKNAME 1

/* Define to 1 if you have the `getspent' function. */
/* #undef HAVE_GETSPENT (moxnox) */

/* Define to 1 if you have the `getspnam' function. */
/* #undef HAVE_GETSPNAM (moxnox) */

/* Define to 1 if you have the `getuid' function. */
#define HAVE_GETUID 1

/* Define to 1 if you have the `getwd' function. */
/* #undef HAVE_GETWD (moxnox) */

/* Define if glibc has incorrect _FORTIFY_SOURCE wrappers for memmove and
   bcopy. */
/* #undef HAVE_GLIBC_MEMMOVE_BUG */

/* Define to 1 if you have the `grantpt' function. */
/* #undef HAVE_GRANTPT (moxnox) */

/* Define to 1 if you have the <grp.h> header file. */
/* #undef HAVE_GRP_H (moxnox) */

/* Define if you have the 'hstrerror' function. */
/* #undef HAVE_HSTRERROR (moxnox) */

/* Define this if you have le64toh() */
#define HAVE_HTOLE64 1

/* Define to 1 if you have the `if_nameindex' function. */
/* #undef HAVE_IF_NAMEINDEX (moxnox) */

/* Define if you have the 'inet_aton' function. */
/* #undef HAVE_INET_ATON (moxnox) */

/* Define if you have the 'inet_ntoa' function. */
/* #undef HAVE_INET_NTOA (moxnox) */

/* Define if you have the 'inet_pton' function. */
/* #undef HAVE_INET_PTON (moxnox) */

/* Define to 1 if you have the `initgroups' function. */
/* #undef HAVE_INITGROUPS (moxnox) */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <io.h> header file. */
/* #undef HAVE_IO_H */

/* Define if gcc has the ipa-pure-const bug. */
/* #undef HAVE_IPA_PURE_CONST_BUG */

/* Define to 1 if you have the `kill' function. */
/* #undef HAVE_KILL (moxnox) */

/* Define to 1 if you have the `killpg' function. */
/* #undef HAVE_KILLPG (moxnox) */

/* Define if you have the 'kqueue' function. */
/* #undef HAVE_KQUEUE */

/* Define to 1 if you have the <langinfo.h> header file. */
/* #undef HAVE_LANGINFO_H (moxnox) */

/* Defined to enable large file support when an off_t is bigger than a long
   and long long is at least as big as an off_t. You may need to add some
   flags for configuration and compilation to enable this mode. (For Solaris
   and Linux, the necessary defines are already defined.) */
/* #undef HAVE_LARGEFILE_SUPPORT */

/* Define to 1 if you have the 'lchflags' function. */
/* #undef HAVE_LCHFLAGS */

/* Define to 1 if you have the `lchmod' function. */
/* #undef HAVE_LCHMOD */

/* Define to 1 if you have the `lchown' function. */
/* #undef HAVE_LCHOWN (moxnox) */

/* Define to 1 if you want to build _blake2 module with libb2 */
/* #undef HAVE_LIBB2 (moxnox: no libb2; _blake2 uses its vendored impl) */

/* Define to 1 if you have the `db' library (-ldb). */
/* #undef HAVE_LIBDB */

/* Define to 1 if you have the `dl' library (-ldl). */
/* #undef HAVE_LIBDL (moxnox) */

/* Define to 1 if you have the `dld' library (-ldld). */
/* #undef HAVE_LIBDLD */

/* Define to 1 if you have the `ieee' library (-lieee). */
/* #undef HAVE_LIBIEEE */

/* Define to 1 if you have the <libintl.h> header file. */
/* #undef HAVE_LIBINTL_H (moxnox) */

/* Define to 1 if you have the `resolv' library (-lresolv). */
/* #undef HAVE_LIBRESOLV */

/* Define to 1 if you have the `sendfile' library (-lsendfile). */
/* #undef HAVE_LIBSENDFILE */

/* Define to 1 if you have the `sqlite3' library (-lsqlite3). */
/* #undef HAVE_LIBSQLITE3 (moxnox) */

/* Define to 1 if you have the <libutil.h> header file. */
/* #undef HAVE_LIBUTIL_H */

/* Define if you have the 'link' function. */
/* #undef HAVE_LINK (moxnox) */

/* Define to 1 if you have the `linkat' function. */
#define HAVE_LINKAT 1

/* Define to 1 if you have the <linux/auxvec.h> header file. */
/* #undef HAVE_LINUX_AUXVEC_H (moxnox) */

/* Define to 1 if you have the <linux/can/bcm.h> header file. */
/* #undef HAVE_LINUX_CAN_BCM_H (moxnox) */

/* Define to 1 if you have the <linux/can.h> header file. */
/* #undef HAVE_LINUX_CAN_H (moxnox) */

/* Define to 1 if you have the <linux/can/j1939.h> header file. */
/* #undef HAVE_LINUX_CAN_J1939_H (moxnox) */

/* Define if compiling using Linux 3.6 or later. */
/* #undef HAVE_LINUX_CAN_RAW_FD_FRAMES (moxnox) */

/* Define to 1 if you have the <linux/can/raw.h> header file. */
/* #undef HAVE_LINUX_CAN_RAW_H (moxnox) */

/* Define if compiling using Linux 4.1 or later. */
/* #undef HAVE_LINUX_CAN_RAW_JOIN_FILTERS (moxnox) */

/* Define to 1 if you have the <linux/fs.h> header file. */
/* #undef HAVE_LINUX_FS_H (moxnox) */

/* Define to 1 if you have the <linux/limits.h> header file. */
/* #undef HAVE_LINUX_LIMITS_H (moxnox) */

/* Define to 1 if you have the <linux/memfd.h> header file. */
/* #undef HAVE_LINUX_MEMFD_H (moxnox) */

/* Define to 1 if you have the <linux/netlink.h> header file. */
/* #undef HAVE_LINUX_NETLINK_H (moxnox) */

/* Define to 1 if you have the <linux/qrtr.h> header file. */
/* #undef HAVE_LINUX_QRTR_H (moxnox) */

/* Define to 1 if you have the <linux/random.h> header file. */
/* #undef HAVE_LINUX_RANDOM_H (moxnox) */

/* Define to 1 if you have the <linux/soundcard.h> header file. */
#define HAVE_LINUX_SOUNDCARD_H 1

/* Define to 1 if you have the <linux/tipc.h> header file. */
/* #undef HAVE_LINUX_TIPC_H (moxnox) */

/* Define to 1 if you have the <linux/vm_sockets.h> header file. */
/* #undef HAVE_LINUX_VM_SOCKETS_H (moxnox) */

/* Define to 1 if you have the <linux/wait.h> header file. */
/* #undef HAVE_LINUX_WAIT_H (moxnox) */

/* Define if you have the 'listen' function. */
#define HAVE_LISTEN 1

/* Define to 1 if you have the `lockf' function. */
/* #undef HAVE_LOCKF (moxnox) */

/* Define to 1 if you have the `log1p' function. */
#define HAVE_LOG1P 1

/* Define to 1 if you have the `log2' function. */
#define HAVE_LOG2 1

/* Define to 1 if you have the `login_tty' function. */
/* #undef HAVE_LOGIN_TTY (moxnox) */

/* Define to 1 if the system has the type `long double'. */
#define HAVE_LONG_DOUBLE 1

/* Define to 1 if you have the `lstat' function. */
#define HAVE_LSTAT 1

/* Define to 1 if you have the `lutimes' function. */
/* #undef HAVE_LUTIMES (moxnox) */

/* Define to 1 if you have the <lzma.h> header file. */
/* #undef HAVE_LZMA_H */

/* Define to 1 if you have the `madvise' function. */
/* #undef HAVE_MADVISE (moxnox) */

/* Define this if you have the makedev macro. */
/* #undef HAVE_MAKEDEV (moxnox) */

/* Define if you have the 'MAXLOGNAME' constant. */
/* #undef HAVE_MAXLOGNAME */

/* Define to 1 if you have the `mbrtowc' function. */
#define HAVE_MBRTOWC 1

/* Define if you have the 'memfd_create' function. */
/* #undef HAVE_MEMFD_CREATE (moxnox) */

/* Define to 1 if you have the `memrchr' function. */
/* #undef HAVE_MEMRCHR (moxnox) */

/* Define to 1 if you have the <minix/config.h> header file. */
/* #undef HAVE_MINIX_CONFIG_H */

/* Define to 1 if you have the `mkdirat' function. */
/* #undef HAVE_MKDIRAT (moxnox) */

/* Define to 1 if you have the `mkfifo' function. */
/* #undef HAVE_MKFIFO (moxnox) */

/* Define to 1 if you have the `mkfifoat' function. */
/* #undef HAVE_MKFIFOAT (moxnox) */

/* Define to 1 if you have the `mknod' function. */
/* #undef HAVE_MKNOD (moxnox) */

/* Define to 1 if you have the `mknodat' function. */
/* #undef HAVE_MKNODAT (moxnox) */

/* Define to 1 if you have the `mktime' function. */
#define HAVE_MKTIME 1

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* Define to 1 if you have the `mremap' function. */
/* #undef HAVE_MREMAP (moxnox) */

/* Define to 1 if you have the `nanosleep' function. */
#define HAVE_NANOSLEEP 1

/* Define if you have the 'ncurses' library */
/* #undef HAVE_NCURSES */

/* Define if you have the 'ncursesw' library */
/* #undef HAVE_NCURSESW (moxnox: the ncurses port is narrow, not wide) */

/* Define to 1 if you have the <ncursesw/curses.h> header file. */
/* #undef HAVE_NCURSESW_CURSES_H (moxnox: use plain <curses.h>) */

/* Define to 1 if you have the <ncursesw/ncurses.h> header file. */
/* #undef HAVE_NCURSESW_NCURSES_H (moxnox: use plain <curses.h>) */

/* Define to 1 if you have the <ncursesw/panel.h> header file. */
/* #undef HAVE_NCURSESW_PANEL_H (moxnox) */

/* Define to 1 if you have the <ncurses/curses.h> header file. */
/* #undef HAVE_NCURSES_CURSES_H */

/* Define to 1 if you have the <ncurses.h> header file. */
/* #undef HAVE_NCURSES_H (moxnox) */

/* Define to 1 if you have the <ncurses/ncurses.h> header file. */
/* #undef HAVE_NCURSES_NCURSES_H */

/* Define to 1 if you have the <ncurses/panel.h> header file. */
/* #undef HAVE_NCURSES_PANEL_H */

/* Define to 1 if you have the <ndbm.h> header file. */
/* #undef HAVE_NDBM_H */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netcan/can.h> header file. */
/* #undef HAVE_NETCAN_CAN_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netlink/netlink.h> header file. */
/* #undef HAVE_NETLINK_NETLINK_H */

/* Define to 1 if you have the <netpacket/packet.h> header file. */
/* #undef HAVE_NETPACKET_PACKET_H (moxnox) */

/* Define to 1 if you have the <net/ethernet.h> header file. */
/* #undef HAVE_NET_ETHERNET_H (moxnox) */

/* Define to 1 if you have the <net/if.h> header file. */
/* #undef HAVE_NET_IF_H (moxnox) */

/* Define to 1 if you have the `nice' function. */
/* #undef HAVE_NICE (moxnox) */

/* Define if the internal form of wchar_t in non-Unicode locales is not
   Unicode. */
/* #undef HAVE_NON_UNICODE_WCHAR_T_REPRESENTATION */

/* Define to 1 if you have the `openat' function. */
/* #undef HAVE_OPENAT (moxnox) */

/* Define to 1 if you have the `opendir' function. */
#define HAVE_OPENDIR 1

/* Define to 1 if you have the `openpty' function. */
/* #undef HAVE_OPENPTY (moxnox) */

/* Define if you have the 'panel' library */
/* #undef HAVE_PANEL */

/* Define if you have the 'panelw' library */
#define HAVE_PANELW 1

/* Define to 1 if you have the <panel.h> header file. */
#define HAVE_PANEL_H 1

/* Define to 1 if you have the `pathconf' function. */
/* #undef HAVE_PATHCONF (moxnox) */

/* Define to 1 if you have the `pause' function. */
/* #undef HAVE_PAUSE (moxnox) */

/* Define to 1 if you have the `pipe' function. */
#define HAVE_PIPE 1

/* Define to 1 if you have the `pipe2' function. */
/* #undef HAVE_PIPE2 (moxnox) */

/* Define to 1 if you have the `plock' function. */
/* #undef HAVE_PLOCK */

/* Define to 1 if you have the `poll' function. */
#define HAVE_POLL 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have the `posix_fadvise' function. */
/* #undef HAVE_POSIX_FADVISE (moxnox) */

/* Define to 1 if you have the `posix_fallocate' function. */
/* #undef HAVE_POSIX_FALLOCATE (moxnox) */

/* Define to 1 if you have the `posix_openpt' function. */
/* #undef HAVE_POSIX_OPENPT (moxnox) */

/* Define to 1 if you have the `posix_spawn' function. */
/* #undef HAVE_POSIX_SPAWN (moxnox) */

/* Define to 1 if you have the `posix_spawnp' function. */
/* #undef HAVE_POSIX_SPAWNP (moxnox) */

/* Define to 1 if you have the `posix_spawn_file_actions_addclosefrom_np'
   function. */
#define HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCLOSEFROM_NP 1

/* Define to 1 if you have the `pread' function. */
/* #undef HAVE_PREAD (moxnox) */

/* Define to 1 if you have the `preadv' function. */
/* #undef HAVE_PREADV (moxnox) */

/* Define to 1 if you have the `preadv2' function. */
/* #undef HAVE_PREADV2 (moxnox) */

/* Define if you have the 'prlimit' function. */
/* #undef HAVE_PRLIMIT (moxnox) */

/* Define to 1 if you have the <process.h> header file. */
/* #undef HAVE_PROCESS_H */

/* Define to 1 if you have the `process_vm_readv' function. */
/* #undef HAVE_PROCESS_VM_READV (moxnox) */

/* Define if your compiler supports function prototype */
#define HAVE_PROTOTYPES 1

/* Define to 1 if you have the `pthread_condattr_setclock' function. */
/* #undef HAVE_PTHREAD_CONDATTR_SETCLOCK (moxnox) */

/* Define to 1 if you have the `pthread_cond_timedwait_relative_np' function.
   */
/* #undef HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP */

/* Defined for Solaris 2.6 bug in pthread header. */
/* #undef HAVE_PTHREAD_DESTRUCTOR */

/* Define to 1 if you have the `pthread_getcpuclockid' function. */
/* #undef HAVE_PTHREAD_GETCPUCLOCKID (moxnox) */

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `pthread_init' function. */
/* #undef HAVE_PTHREAD_INIT */

/* Define to 1 if you have the `pthread_kill' function. */
/* #undef HAVE_PTHREAD_KILL (moxnox) */

/* Define to 1 if you have the `pthread_sigmask' function. */
/* #undef HAVE_PTHREAD_SIGMASK (moxnox) */

/* Define if platform requires stubbed pthreads support */
/* #undef HAVE_PTHREAD_STUBS */

/* Define to 1 if you have the `ptsname' function. */
/* #undef HAVE_PTSNAME (moxnox) */

/* Define to 1 if you have the `ptsname_r' function. */
/* #undef HAVE_PTSNAME_R (moxnox) */

/* Define to 1 if you have the <pty.h> header file. */
/* #undef HAVE_PTY_H (moxnox) */

/* Define to 1 if you have the `pwrite' function. */
/* #undef HAVE_PWRITE (moxnox) */

/* Define to 1 if you have the `pwritev' function. */
/* #undef HAVE_PWRITEV (moxnox) */

/* Define to 1 if you have the `pwritev2' function. */
/* #undef HAVE_PWRITEV2 (moxnox) */

/* Define to 1 if you have the <readline/readline.h> header file. */
/* #undef HAVE_READLINE_READLINE_H */

/* Define to 1 if you have the `readlink' function. */
/* #undef HAVE_READLINK (moxnox) */

/* Define to 1 if you have the `readlinkat' function. */
/* #undef HAVE_READLINKAT (moxnox) */

/* Define to 1 if you have the `readv' function. */
/* #undef HAVE_READV (moxnox) */

/* Define to 1 if you have the `realpath' function. */
/* #undef HAVE_REALPATH (moxnox) */

/* Define if you have the 'recvfrom' function. */
#define HAVE_RECVFROM 1

/* Define to 1 if you have the `renameat' function. */
/* #undef HAVE_RENAMEAT (moxnox) */

/* Define if readline supports append_history */
/* #undef HAVE_RL_APPEND_HISTORY (moxnox) */

/* Define if you can turn off readline's signal handling. */
/* #undef HAVE_RL_CATCH_SIGNAL (moxnox) */

/* Define to 1 if the system has the type `rl_compdisp_func_t'. */
#define HAVE_RL_COMPDISP_FUNC_T 1

/* Define if you have readline 2.2 */
#define HAVE_RL_COMPLETION_APPEND_CHARACTER 1

/* Define if you have readline 4.0 */
#define HAVE_RL_COMPLETION_DISPLAY_MATCHES_HOOK 1

/* Define if you have readline 4.2 */
#define HAVE_RL_COMPLETION_MATCHES 1

/* Define if you have rl_completion_suppress_append */
#define HAVE_RL_COMPLETION_SUPPRESS_APPEND 1

/* Define if you have readline 4.0 */
#define HAVE_RL_PRE_INPUT_HOOK 1

/* Define if you have readline 4.0 */
#define HAVE_RL_RESIZE_TERMINAL 1

/* Define to 1 if you have the `rtpSpawn' function. */
/* #undef HAVE_RTPSPAWN */

/* Define to 1 if you have the `sched_get_priority_max' function. */
/* #undef HAVE_SCHED_GET_PRIORITY_MAX (moxnox) */

/* Define to 1 if you have the <sched.h> header file. */
/* #undef HAVE_SCHED_H (moxnox) */

/* Define to 1 if you have the `sched_rr_get_interval' function. */
/* #undef HAVE_SCHED_RR_GET_INTERVAL (moxnox) */

/* Define to 1 if you have the `sched_setaffinity' function. */
/* #undef HAVE_SCHED_SETAFFINITY (moxnox) */

/* Define to 1 if you have the `sched_setparam' function. */
/* #undef HAVE_SCHED_SETPARAM (moxnox) */

/* Define to 1 if you have the `sched_setscheduler' function. */
/* #undef HAVE_SCHED_SETSCHEDULER (moxnox) */

/* Define to 1 if you have the `sem_clockwait' function. */
/* #undef HAVE_SEM_CLOCKWAIT (moxnox) */

/* Define to 1 if you have the `sem_getvalue' function. */
/* #undef HAVE_SEM_GETVALUE (moxnox) */

/* Define to 1 if you have the `sem_open' function. */
/* #undef HAVE_SEM_OPEN (moxnox) */

/* Define to 1 if you have the `sem_timedwait' function. */
/* #undef HAVE_SEM_TIMEDWAIT (moxnox) */

/* Define to 1 if you have the `sem_unlink' function. */
/* #undef HAVE_SEM_UNLINK (moxnox) */

/* Define to 1 if you have the `sendfile' function. */
/* #undef HAVE_SENDFILE (moxnox) */

/* Define if you have the 'sendto' function. */
#define HAVE_SENDTO 1

/* Define to 1 if you have the `setegid' function. */
/* #undef HAVE_SETEGID (moxnox) */

/* Define to 1 if you have the `seteuid' function. */
/* #undef HAVE_SETEUID (moxnox) */

/* Define to 1 if you have the `setgid' function. */
/* #undef HAVE_SETGID (moxnox) */

/* Define if you have the 'setgroups' function. */
/* #undef HAVE_SETGROUPS (moxnox) */

/* Define to 1 if you have the `sethostname' function. */
/* #undef HAVE_SETHOSTNAME (moxnox) */

/* Define to 1 if you have the `setitimer' function. */
/* #undef HAVE_SETITIMER (moxnox) */

/* Define to 1 if you have the <setjmp.h> header file. */
#define HAVE_SETJMP_H 1

/* Define to 1 if you have the `setlocale' function. */
/* #undef HAVE_SETLOCALE (moxnox) */

/* Define to 1 if you have the `setns' function. */
/* #undef HAVE_SETNS (moxnox) */

/* Define to 1 if you have the `setpgid' function. */
/* #undef HAVE_SETPGID (moxnox) */

/* Define to 1 if you have the `setpgrp' function. */
/* #undef HAVE_SETPGRP (moxnox) */

/* Define to 1 if you have the `setpriority' function. */
/* #undef HAVE_SETPRIORITY (moxnox) */

/* Define to 1 if you have the `setregid' function. */
/* #undef HAVE_SETREGID (moxnox) */

/* Define to 1 if you have the `setresgid' function. */
/* #undef HAVE_SETRESGID (moxnox) */

/* Define to 1 if you have the `setresuid' function. */
/* #undef HAVE_SETRESUID (moxnox) */

/* Define to 1 if you have the `setreuid' function. */
/* #undef HAVE_SETREUID (moxnox) */

/* Define to 1 if you have the `setsid' function. */
/* #undef HAVE_SETSID (moxnox) */

/* Define if you have the 'setsockopt' function. */
#define HAVE_SETSOCKOPT 1

/* Define to 1 if you have the `setuid' function. */
/* #undef HAVE_SETUID (moxnox) */

/* Define to 1 if you have the `setvbuf' function. */
#define HAVE_SETVBUF 1

/* Define to 1 if you have the <shadow.h> header file. */
#define HAVE_SHADOW_H 1

/* Define to 1 if you have the `shm_open' function. */
#define HAVE_SHM_OPEN 1

/* Define to 1 if you have the `shm_unlink' function. */
#define HAVE_SHM_UNLINK 1

/* Define to 1 if you have the `shutdown' function. */
#define HAVE_SHUTDOWN 1

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the `sigaltstack' function. */
/* #undef HAVE_SIGALTSTACK (moxnox) */

/* Define to 1 if you have the `sigfillset' function. */
/* #undef HAVE_SIGFILLSET (moxnox) */

/* Define to 1 if `si_band' is a member of `siginfo_t'. */
/* #undef HAVE_SIGINFO_T_SI_BAND (moxnox) */

/* Define to 1 if you have the `siginterrupt' function. */
/* #undef HAVE_SIGINTERRUPT (moxnox) */

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the `sigpending' function. */
/* #undef HAVE_SIGPENDING (moxnox) */

/* Define to 1 if you have the `sigrelse' function. */
/* #undef HAVE_SIGRELSE (moxnox) */

/* Define to 1 if you have the `sigtimedwait' function. */
/* #undef HAVE_SIGTIMEDWAIT (moxnox) */

/* Define to 1 if you have the `sigwait' function. */
/* #undef HAVE_SIGWAIT (moxnox) */

/* Define to 1 if you have the `sigwaitinfo' function. */
/* #undef HAVE_SIGWAITINFO (moxnox) */

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* struct sockaddr_alg (linux/if_alg.h) */
/* #undef HAVE_SOCKADDR_ALG (moxnox) */

/* Define if sockaddr has sa_len member */
/* #undef HAVE_SOCKADDR_SA_LEN */

/* struct sockaddr_storage (sys/socket.h) */
#define HAVE_SOCKADDR_STORAGE 1

/* Define if you have the 'socket' function. */
#define HAVE_SOCKET 1

/* Define if you have the 'socketpair' function. */
/* #undef HAVE_SOCKETPAIR (moxnox) */

/* Define to 1 if the system has the type `socklen_t'. */
#define HAVE_SOCKLEN_T 1

/* Define to 1 if you have the <spawn.h> header file. */
#define HAVE_SPAWN_H 1

/* Define to 1 if you have the `splice' function. */
/* #undef HAVE_SPLICE (moxnox) */

/* Define to 1 if the system has the type `ssize_t'. */
#define HAVE_SSIZE_T 1

/* Define to 1 if you have the `statvfs' function. */
/* #undef HAVE_STATVFS (moxnox) */

/* Define if you have struct stat.st_mtim.tv_nsec */
/* #undef HAVE_STAT_TV_NSEC (moxnox) */

/* Define if you have struct stat.st_mtimensec */
/* #undef HAVE_STAT_TV_NSEC2 */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Has stdatomic.h with atomic_int and atomic_uintptr_t */
#define HAVE_STD_ATOMIC 1

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the <stropts.h> header file. */
/* #undef HAVE_STROPTS_H */

/* Define to 1 if you have the `strsignal' function. */
/* #undef HAVE_STRSIGNAL (moxnox) */

/* Define to 1 if `pw_gecos' is a member of `struct passwd'. */
/* #undef HAVE_STRUCT_PASSWD_PW_GECOS (moxnox) */

/* Define to 1 if `pw_passwd' is a member of `struct passwd'. */
/* #undef HAVE_STRUCT_PASSWD_PW_PASSWD (moxnox) */

/* Define to 1 if `st_birthtime' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_BIRTHTIME */

/* Define to 1 if `st_blksize' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_BLKSIZE (moxnox) */

/* Define to 1 if `st_blocks' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_BLOCKS (moxnox) */

/* Define to 1 if `st_flags' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_FLAGS */

/* Define to 1 if `st_gen' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_GEN */

/* Define to 1 if `st_rdev' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_RDEV (moxnox) */

/* Define to 1 if `tm_zone' is a member of `struct tm'. */
/* #undef HAVE_STRUCT_TM_TM_ZONE (moxnox) */

/* Define if you have the 'symlink' function. */
/* #undef HAVE_SYMLINK (moxnox) */

/* Define to 1 if you have the `symlinkat' function. */
/* #undef HAVE_SYMLINKAT (moxnox) */

/* Define to 1 if you have the `sync' function. */
/* #undef HAVE_SYNC (moxnox) */

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define to 1 if you have the <sysexits.h> header file. */
/* #undef HAVE_SYSEXITS_H (moxnox) */

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the `system' function. */
#define HAVE_SYSTEM 1

/* Define to 1 if you have the <sys/audioio.h> header file. */
/* #undef HAVE_SYS_AUDIOIO_H */

/* Define to 1 if you have the <sys/auxv.h> header file. */
#define HAVE_SYS_AUXV_H 1

/* Define to 1 if you have the <sys/bsdtty.h> header file. */
/* #undef HAVE_SYS_BSDTTY_H */

/* Define to 1 if you have the <sys/devpoll.h> header file. */
/* #undef HAVE_SYS_DEVPOLL_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/endian.h> header file. */
/* #undef HAVE_SYS_ENDIAN_H */

/* Define to 1 if you have the <sys/epoll.h> header file. */
/* #undef HAVE_SYS_EPOLL_H (moxnox) */

/* Define to 1 if you have the <sys/eventfd.h> header file. */
/* #undef HAVE_SYS_EVENTFD_H (moxnox) */

/* Define to 1 if you have the <sys/event.h> header file. */
/* #undef HAVE_SYS_EVENT_H */

/* Define to 1 if you have the <sys/file.h> header file. */
/* #undef HAVE_SYS_FILE_H (moxnox) */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
/* #undef HAVE_SYS_IOCTL_H (moxnox) */

/* Define to 1 if you have the <sys/kern_control.h> header file. */
/* #undef HAVE_SYS_KERN_CONTROL_H */

/* Define to 1 if you have the <sys/loadavg.h> header file. */
/* #undef HAVE_SYS_LOADAVG_H */

/* Define to 1 if you have the <sys/lock.h> header file. */
/* #undef HAVE_SYS_LOCK_H */

/* Define to 1 if you have the <sys/memfd.h> header file. */
/* #undef HAVE_SYS_MEMFD_H */

/* Define to 1 if you have the <sys/mkdev.h> header file. */
/* #undef HAVE_SYS_MKDEV_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/modem.h> header file. */
/* #undef HAVE_SYS_MODEM_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/pidfd.h> header file. */
/* #undef HAVE_SYS_PIDFD_H (moxnox) */

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/random.h> header file. */
/* #undef HAVE_SYS_RANDOM_H (moxnox) */

/* Define to 1 if you have the <sys/resource.h> header file. */
/* #undef HAVE_SYS_RESOURCE_H (moxnox) */

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/sendfile.h> header file. */
/* #undef HAVE_SYS_SENDFILE_H (moxnox) */

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/soundcard.h> header file. */
#define HAVE_SYS_SOUNDCARD_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. */
/* #undef HAVE_SYS_STATVFS_H (moxnox) */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/syscall.h> header file. */
/* #undef HAVE_SYS_SYSCALL_H (moxnox) */

/* Define to 1 if you have the <sys/sysmacros.h> header file. */
/* #undef HAVE_SYS_SYSMACROS_H (moxnox) */

/* Define to 1 if you have the <sys/sys_domain.h> header file. */
/* #undef HAVE_SYS_SYS_DOMAIN_H */

/* Define to 1 if you have the <sys/termio.h> header file. */
/* #undef HAVE_SYS_TERMIO_H */

/* Define to 1 if you have the <sys/timerfd.h> header file. */
/* #undef HAVE_SYS_TIMERFD_H (moxnox) */

/* Define to 1 if you have the <sys/times.h> header file. */
#define HAVE_SYS_TIMES_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
/* #undef HAVE_SYS_UIO_H (moxnox) */

/* Define to 1 if you have the <sys/un.h> header file. */
/* #undef HAVE_SYS_UN_H (moxnox) */

/* Define to 1 if you have the <sys/utsname.h> header file. */
/* #undef HAVE_SYS_UTSNAME_H (moxnox) */

/* Define to 1 if you have the <sys/wait.h> header file. */
/* #undef HAVE_SYS_WAIT_H (moxnox) */

/* Define to 1 if you have the <sys/xattr.h> header file. */
/* #undef HAVE_SYS_XATTR_H (moxnox) */

/* Define to 1 if you have the `tcgetpgrp' function. */
/* #undef HAVE_TCGETPGRP (moxnox) */

/* Define to 1 if you have the `tcsetpgrp' function. */
/* #undef HAVE_TCSETPGRP (moxnox) */

/* Define to 1 if you have the `tempnam' function. */
#define HAVE_TEMPNAM 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if you have the <term.h> header file. */
#define HAVE_TERM_H 1

/* Define to 1 if you have the `timegm' function. */
/* #undef HAVE_TIMEGM (moxnox) */

/* Define if you have the 'timerfd_create' function. */
/* #undef HAVE_TIMERFD_CREATE (moxnox) */

/* Define to 1 if you have the `times' function. */
#define HAVE_TIMES 1

/* Define to 1 if you have the `tmpfile' function. */
#define HAVE_TMPFILE 1

/* Define to 1 if you have the `tmpnam' function. */
/* #undef HAVE_TMPNAM (moxnox) */

/* Define to 1 if you have the `tmpnam_r' function. */
/* #undef HAVE_TMPNAM_R (moxnox) */

/* Define to 1 if your `struct tm' has `tm_zone'. Deprecated, use
   `HAVE_STRUCT_TM_TM_ZONE' instead. */
/* #undef HAVE_TM_ZONE (moxnox) */

/* Define to 1 if you have the `truncate' function. */
/* #undef HAVE_TRUNCATE (moxnox) */

/* Define to 1 if you have the `ttyname_r' function. */
/* #undef HAVE_TTYNAME_R (moxnox) */

/* Define to 1 if you don't have `tm_zone' but do have the external array
   `tzname'. */
/* #undef HAVE_TZNAME */

/* Define to 1 if you have the `umask' function. */
#define HAVE_UMASK 1

/* Define to 1 if you have the `uname' function. */
/* #undef HAVE_UNAME (moxnox) */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `unlinkat' function. */
/* #undef HAVE_UNLINKAT (moxnox) */

/* Define to 1 if you have the `unlockpt' function. */
/* #undef HAVE_UNLOCKPT (moxnox) */

/* Define to 1 if you have the `unshare' function. */
/* #undef HAVE_UNSHARE (moxnox) */

/* Define if you have a useable wchar_t type defined in wchar.h; useable means
   wchar_t must be an unsigned type with at least 16 bits. (see
   Include/unicodeobject.h). */
/* #undef HAVE_USABLE_WCHAR_T */

/* Define to 1 if you have the <util.h> header file. */
/* #undef HAVE_UTIL_H */

/* Define to 1 if you have the `utimensat' function. */
/* #undef HAVE_UTIMENSAT (moxnox) */

/* Define to 1 if you have the `utimes' function. */
/* #undef HAVE_UTIMES (moxnox) */

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if you have the <utmp.h> header file. */
#define HAVE_UTMP_H 1

/* Define if you have the 'HAVE_UT_NAMESIZE' constant. */
#define HAVE_UT_NAMESIZE 1

/* Define to 1 if you have the `uuid_create' function. */
/* #undef HAVE_UUID_CREATE */

/* Define to 1 if you have the `uuid_enc_be' function. */
/* #undef HAVE_UUID_ENC_BE */

/* Define if uuid_generate_time_safe() exists. */
#define HAVE_UUID_GENERATE_TIME_SAFE 1

/* Define if uuid_generate_time_safe() is able to deduce a MAC address. */
/* #undef HAVE_UUID_GENERATE_TIME_SAFE_STABLE_MAC */

/* Define to 1 if you have the <uuid.h> header file. */
#define HAVE_UUID_H 1

/* Define to 1 if you have the <uuid/uuid.h> header file. */
/* #undef HAVE_UUID_UUID_H */

/* Define to 1 if you have the `vfork' function. */
/* #undef HAVE_VFORK (moxnox) */

/* Define to 1 if you have the `wait' function. */
/* #undef HAVE_WAIT (moxnox) */

/* Define to 1 if you have the `wait3' function. */
/* #undef HAVE_WAIT3 (moxnox) */

/* Define to 1 if you have the `wait4' function. */
/* #undef HAVE_WAIT4 (moxnox) */

/* Define to 1 if you have the `waitid' function. */
/* #undef HAVE_WAITID (moxnox) */

/* Define to 1 if you have the `waitpid' function. */
/* #undef HAVE_WAITPID (moxnox) */

/* Define if the compiler provides a wchar.h header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the `wcscoll' function. */
/* #undef HAVE_WCSCOLL (moxnox) */

/* Define to 1 if you have the `wcsftime' function. */
/* #undef HAVE_WCSFTIME (moxnox) */

/* Define to 1 if you have the `wcsxfrm' function. */
/* #undef HAVE_WCSXFRM (moxnox) */

/* Define to 1 if you have the `wmemcmp' function. */
#define HAVE_WMEMCMP 1

/* Define if tzset() actually switches the local timezone in a meaningful way.
   */
/* #undef HAVE_WORKING_TZSET (moxnox) */

/* Define to 1 if you have the `writev' function. */
/* #undef HAVE_WRITEV (moxnox) */

/* Define if the zlib library has inflateCopy */
/* #undef HAVE_ZLIB_COPY (moxnox) */

/* Define to 1 if you have the <zlib.h> header file. */
/* #undef HAVE_ZLIB_H */

/* Define to 1 if you have the `_getpty' function. */
/* #undef HAVE__GETPTY */

/* Define to 1 if the system has the type `__uint128_t'. */
#define HAVE___UINT128_T 1

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS (moxnox) */

/* Define if mvwdelch in curses.h is an expression. */
#define MVWDELCH_IS_EXPRESSION 1

/* Define to the address where bug reports for this package should be sent. */
/* #undef PACKAGE_BUGREPORT */

/* Define to the full name of this package. */
/* #undef PACKAGE_NAME */

/* Define to the full name and version of this package. */
/* #undef PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
/* #undef PACKAGE_TARNAME */

/* Define to the home page for this package. */
/* #undef PACKAGE_URL */

/* Define to the version of this package. */
/* #undef PACKAGE_VERSION */

/* Define if POSIX semaphores aren't enabled on your system */
/* #undef POSIX_SEMAPHORES_NOT_ENABLED */

/* Define if pthread_key_t is compatible with int. */
#define PTHREAD_KEY_T_IS_COMPATIBLE_WITH_INT 1

/* Defined if PTHREAD_SCOPE_SYSTEM supported. */
/* #undef PTHREAD_SYSTEM_SCHED_SUPPORTED (moxnox) */

/* Define as the preferred size in bits of long digits */
/* #undef PYLONG_BITS_IN_DIGIT */

/* enabled builtin hash modules */
#define PY_BUILTIN_HASHLIB_HASHES "md5,sha1,sha2,sha3,blake2"

/* Define if you want to coerce the C locale to a UTF-8 based locale */
#define PY_COERCE_C_LOCALE 1

/* Define to 1 if you have the perf trampoline. */
/* #undef PY_HAVE_PERF_TRAMPOLINE (moxnox) */

/* Define to 1 to build the sqlite module with loadable extensions support. */
/* #undef PY_SQLITE_ENABLE_LOAD_EXTENSION */

/* Define if SQLite was compiled with the serialize API */
#define PY_SQLITE_HAVE_SERIALIZE 1

/* Default cipher suites list for ssl module. 1: Python's preferred selection,
   2: leave OpenSSL defaults untouched, 0: custom string */
#define PY_SSL_DEFAULT_CIPHERS 1

/* Cipher suite string for PY_SSL_DEFAULT_CIPHERS=0 */
/* #undef PY_SSL_DEFAULT_CIPHER_STRING */

/* PEP 11 Support tier (1, 2, 3 or 0 for unsupported) */
#define PY_SUPPORT_TIER 0

/* Define if you want to build an interpreter with many run-time checks. */
/* #undef Py_DEBUG */

/* Defined if Python is built as a shared library. */
/* #undef Py_ENABLE_SHARED */

/* Define if you want to disable the GIL */
/* #undef Py_GIL_DISABLED */

/* Define hash algorithm for str, bytes and memoryview. SipHash24: 1, FNV: 2,
   SipHash13: 3, externally defined: 0 */
/* #undef Py_HASH_ALGORITHM */

/* Define if rl_startup_hook takes arguments */
/* #undef Py_RL_STARTUP_HOOK_TAKES_ARGS */

/* Define if you want to enable internal statistics gathering. */
/* #undef Py_STATS */

/* The version of SunOS/Solaris as reported by `uname -r' without the dot. */
/* #undef Py_SUNOS_VERSION */

/* Define if you want to enable tracing references for debugging purpose */
/* #undef Py_TRACE_REFS */

/* assume C89 semantics that RETSIGTYPE is always void */
#define RETSIGTYPE void

/* Define if setpgrp() must be called as setpgrp(0, 0). */
/* #undef SETPGRP_HAVE_ARG */

/* Define if i>>j for signed int i does not extend the sign bit when i < 0 */
/* #undef SIGNED_RIGHT_SHIFT_ZERO_FILLS */

/* The size of `double', as computed by sizeof. */
#define SIZEOF_DOUBLE 8

/* The size of `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of `fpos_t', as computed by sizeof. */
#define SIZEOF_FPOS_T 16

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of `long double', as computed by sizeof. */
#define SIZEOF_LONG_DOUBLE 16

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `off_t', as computed by sizeof. */
#define SIZEOF_OFF_T 4

/* The size of `pid_t', as computed by sizeof. */
#define SIZEOF_PID_T 4

/* The size of `pthread_key_t', as computed by sizeof. */
#define SIZEOF_PTHREAD_KEY_T 4

/* The size of `pthread_t', as computed by sizeof. */
#define SIZEOF_PTHREAD_T 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 8

/* The size of `time_t', as computed by sizeof. */
#define SIZEOF_TIME_T 8

/* The size of `uintptr_t', as computed by sizeof. */
#define SIZEOF_UINTPTR_T 8

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* The size of `wchar_t', as computed by sizeof. */
#define SIZEOF_WCHAR_T 2

/* The size of `_Bool', as computed by sizeof. */
#define SIZEOF__BOOL 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/select.h> and <sys/time.h>
   (which you can't on SCO ODT 3.0). */
#define SYS_SELECT_WITH_SYS_TIME 1

/* Custom thread stack size depending on chosen sanitizer runtimes. */
/* #undef THREAD_STACK_SIZE */

/* Library needed by timemodule.c: librt may be needed for clock_gettime() */
/* #undef TIMEMODULE_LIB */

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Define if you want to use computed gotos in ceval.c. */
/* #undef USE_COMPUTED_GOTOS */

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
# define _HPUX_ALT_XOPEN_SOCKET_API 1
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
/* # undef _MINIX */
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
# define _NETBSD_SOURCE 1
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
# define _OPENBSD_SOURCE 1
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
/* # undef _POSIX_SOURCE */
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
/* # undef _POSIX_1_SOURCE */
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
# define __STDC_WANT_IEC_60559_ATTRIBS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
# define __STDC_WANT_IEC_60559_BFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
# define __STDC_WANT_IEC_60559_DFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
# define __STDC_WANT_IEC_60559_FUNCS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
# define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
# define __STDC_WANT_LIB_EXT2__ 1
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
# define __STDC_WANT_MATH_SPEC_FUNCS__ 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 700
#endif


/* Define if WINDOW in curses.h offers a field _flags. */
/* #undef WINDOW_HAS_FLAGS */

/* Define if you want build the _decimal module using a coroutine-local rather
   than a thread-local context */
#define WITH_DECIMAL_CONTEXTVAR 1

/* Define if you want documentation strings in extension modules */
#define WITH_DOC_STRINGS 1

/* Define if you want to compile in DTrace support */
/* #undef WITH_DTRACE */

/* Define if you want to use the new-style (Openstep, Rhapsody, MacOS) dynamic
   linker (dyld) instead of the old-style (NextStep) dynamic linker (rld).
   Dyld is necessary to support frameworks. */
/* #undef WITH_DYLD */

/* Define to build the readline module against libedit. */
/* #undef WITH_EDITLINE */

/* Define if you want to compile in object freelists optimization */
#define WITH_FREELISTS 1

/* Define to 1 if libintl is needed for locale functions. */
/* #undef WITH_LIBINTL */

/* Define if you want to compile in mimalloc memory allocator. */
/* #undef WITH_MIMALLOC */

/* Define if you want to produce an OpenStep/Rhapsody framework (shared
   library plus accessory files). */
/* #undef WITH_NEXT_FRAMEWORK */

/* Define if you want to compile in Python-specific mallocs */
#define WITH_PYMALLOC 1

/* Define if you want pymalloc to be disabled when running under valgrind */
/* #undef WITH_VALGRIND */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define if arithmetic is subject to x87-style double rounding issue */
/* #undef X87_DOUBLE_ROUNDING */

/* Define on OpenBSD to activate all library features */
/* #undef _BSD_SOURCE */

/* Define on Darwin to activate all library features */
#define _DARWIN_C_SOURCE 1

/* This must be set to 64 on some systems to enable large file support. */
#define _FILE_OFFSET_BITS 64

/* Define to include mbstate_t for mbrtowc */
/* #undef _INCLUDE__STDC_A1_SOURCE */

/* This must be defined on some systems to enable large file support. */
#define _LARGEFILE_SOURCE 1

/* This must be defined on AIX systems to enable large file support. */
/* #undef _LARGE_FILES */

/* Define on NetBSD to activate all library features */
#define _NETBSD_SOURCE 1

/* Define to activate features from IEEE Stds 1003.1-2008 */
#define _POSIX_C_SOURCE 200809L

/* Define if you have POSIX threads, and your system does not define that. */
/* #undef _POSIX_THREADS */

/* framework name */
#define _PYTHONFRAMEWORK ""

/* Define to force use of thread-safe errno, h_errno, and other functions */
#define _REENTRANT 1

/* Define to 1 if you want to emulate getpid() on WASI */
/* #undef _WASI_EMULATED_GETPID */

/* Define to 1 if you want to emulate process clocks on WASI */
/* #undef _WASI_EMULATED_PROCESS_CLOCKS */

/* Define to 1 if you want to emulate signals on WASI */
/* #undef _WASI_EMULATED_SIGNAL */

/* Define to the level of X/Open that your system supports */
#define _XOPEN_SOURCE 700

/* Define to activate Unix95-and-earlier features */
#define _XOPEN_SOURCE_EXTENDED 1

/* Define on FreeBSD to activate all library features */
#define __BSD_VISIBLE 1

/* Define to 'long' if <time.h> does not define clock_t. */
/* #undef clock_t */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define as a signed integer type capable of holding a process identifier. */
/* #undef pid_t */

/* Define to empty if the keyword does not work. */
/* #undef signed */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to 'int' if <sys/socket.h> does not define. */
/* #undef socklen_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */


/* Define the macros needed if on a UnixWare 7.x system. */
#if defined(__USLC__) && defined(__SCO_VERSION__)
#define STRICT_SYSV_CURSES /* Don't use ncurses extensions */
#endif

#endif /*Py_PYCONFIG_H*/

