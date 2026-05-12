#include "dll_stubs.S"

stubs_start

do_stub "getsockopt"
do_stub "ioctlsocket"
do_stub "getsockname"
do_stub "getpeername"
do_stub "gethostname"
do_stub "select"
do_stub "shutdown"
do_stub "getnameinfo"

stubs_end
