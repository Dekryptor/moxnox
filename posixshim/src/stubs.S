#include "dll_stubs.S"

stubs_start

do_stub_no_export "dlopen"
do_stub_no_export "dlsym"
do_stub_no_export "dlclose"
do_stub_no_export "dlerror"
do_stub_no_export "wait"
do_stub_no_export "link"
do_stub_no_export "uname"
do_stub_no_export "fsync"
do_stub_no_export "getpgid"
do_stub_no_export "tcgetpgrp"
do_stub_no_export "getpass"
do_stub_no_export "statvfs"
do_stub_no_export "getpagesize"
do_stub_no_export "pause"
do_stub_no_export "clock_getres"
do_stub_no_export "times"
do_stub_no_export "pthread_setcancelstate"
do_stub_no_export "pthread_mutexattr_init"
do_stub_no_export "pthread_mutexattr_destroy"
do_stub_no_export "pthread_mutexattr_settype"
do_stub_no_export "pthread_once"
do_stub_no_export "munmap"
do_stub_no_export "msync"

stubs_end
