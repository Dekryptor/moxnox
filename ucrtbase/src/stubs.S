#include "dll_stubs.S"

stubs_start

do_stub "_beginthreadex"
do_stub "fgetpos"
do_stub "fsetpos"
do_stub "wcscat"

stubs_end
