#include "dll_stubs.S"

stubs_start

do_stub "getaddrinfo"
do_stub "freeaddrinfo"
do_stub "gai_strerror"

stubs_end
