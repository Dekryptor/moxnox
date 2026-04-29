#ifdef __x86_64__
.intel_syntax noprefix
#endif

.macro underscore_func name
    .section ".text$\name"
    .global "\name"
    "\name":
#ifdef __x86_64__
    jmp [rip+__imp__\name]
#elif defined(__aarch64__)
    adrp x16, __imp__\name
    ldr x16, [x16, :lo12:__imp__\name]
    br x16
#endif
.endm

# mingw likes to link to functions in ucrtbase that only have an underscored
# form without the leading underscore. Adding a redirect in ucrtbase_static is
# the path of least resistance...

underscore_func "access"
underscore_func "chdir"
underscore_func "close"
underscore_func "fstat64i32"
underscore_func "getcwd"
underscore_func "isatty"
underscore_func "mkdir"
underscore_func "open"
underscore_func "putenv"
underscore_func "stat64i32"
underscore_func "stat64"
underscore_func "stat32i64"
underscore_func "stat32"
underscore_func "strdup"
underscore_func "fileno"
underscore_func "read"
underscore_func "write"
underscore_func "fdopen"
underscore_func "chmod"
underscore_func "utime"
underscore_func "getpid"
underscore_func "stricmp"

.section ".text$fseeko64"
.global fseeko64
fseeko64:
#ifdef __x86_64__
    jmp [rip+__imp__fseeki64]
#elif defined(__aarch64__)
    adrp x16, __imp__fseeki64
    ldr x16, [x16, :lo12:__imp__fseeki64]
    br x16
#endif

.section ".text$ftello64"
.global ftello64
ftello64:
#ifdef __x86_64__
    jmp [rip+__imp__ftelli64]
#elif defined(__aarch64__)
    adrp x16, __imp__ftelli64
    ldr x16, [x16, :lo12:__imp__ftelli64]
    br x16
#endif
