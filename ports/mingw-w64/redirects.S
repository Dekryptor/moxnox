#ifdef __x86_64__
.intel_syntax noprefix
#endif

.macro underscore_func name
    .section ".text$\name","ax"
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

.macro redirect_name new_name old_name
    .section ".text$\old_name","ax"
    .global "\old_name"
    "\old_name":
#ifdef __x86_64__
    jmp [rip+__imp_\new_name]
#elif defined(__aarch64__)
    adrp x16, __imp_\new_name
    ldr x16, [x16, :lo12:__imp_\new_name]
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
underscore_func "open"
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
underscore_func "strnicmp"
underscore_func "wcsnicmp"
underscore_func "unlink"
underscore_func "umask"
underscore_func "lseek"
underscore_func "dup"
underscore_func "rmdir"
underscore_func "pipe"
underscore_func "dup2"
underscore_func "fstat64"
underscore_func "execvp"
underscore_func "execv"
underscore_func "execl"
underscore_func "execlp"
underscore_func "execve"
underscore_func "creat"

redirect_name "_fseeki64", "fseeko64"
redirect_name "_ftelli64", "ftello64"
redirect_name "_stricmp", "strcasecmp"
redirect_name "_strnicmp", "strncasecmp"
redirect_name "_stat64", "stat"
redirect_name "_fstat64", "fstat"
