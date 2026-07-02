#include "dll_stubs.S"

stubs_start

do_stub "dlopen"
do_stub "dlsym"
do_stub "dlclose"
do_stub "dlerror"
do_stub "statvfs"

stubs_end
