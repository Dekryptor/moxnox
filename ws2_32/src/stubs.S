#include "dll_stubs.S"

stubs_start

do_stub "setsockopt"
do_stub "getsockopt"
do_stub "listen"
do_stub "accept"

stubs_end
