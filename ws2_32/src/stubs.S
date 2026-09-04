#include "dll_stubs.S"

stubs_start

do_stub "getservbyname"
do_stub "getservbyport"
do_stub "getprotobyname"
do_stub "getprotobynumber"

stubs_end
