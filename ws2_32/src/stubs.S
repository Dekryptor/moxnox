#include "dll_stubs.S"

stubs_start

do_stub "getsockopt"
do_stub "getpeername"
do_stub "gethostname"
do_stub "shutdown"
do_stub "getnameinfo"

stubs_end
