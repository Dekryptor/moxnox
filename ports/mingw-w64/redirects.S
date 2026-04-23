.macro underscore_func name
    .section ".text$\name"
    .global "\name"
    "\name":
#ifdef __x86_64__
    jmp "_\name"
#elif defined(__aarch64__)
    b "_\name"
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
