#include "dll_stubs.S"

stubs_start

do_stub_no_export "dlopen"
do_stub_no_export "dlsym"
do_stub_no_export "dlclose"
do_stub_no_export "dlerror"
do_stub_no_export "link"
do_stub_no_export "uname"
do_stub_no_export "getpgid"
do_stub_no_export "tcgetpgrp"
do_stub_no_export "getpass"
do_stub_no_export "statvfs"
do_stub_no_export "times"

stubs_end
