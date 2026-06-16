#include "dll_stubs.S"

stubs_start

do_stub "getpeername"
do_stub "shutdown"
do_stub "getnameinfo"
do_stub "gethostbyname"
do_stub "getservbyname"

stubs_end
