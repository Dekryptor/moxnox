#include "dll_stubs.S"

stubs_start

do_stub "dlopen"
do_stub "dlsym"
do_stub "dlclose"
do_stub "dlerror"
do_stub "link"
do_stub "getpgid"
do_stub "tcgetpgrp"
do_stub "getpass"
do_stub "statvfs"
do_stub "times"

stubs_end
