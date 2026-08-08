#include "dll_stubs.S"

stubs_start

do_stub "_beginthreadex"
do_stub "fgetpos"
do_stub "fsetpos"
do_stub "wcscat"
do_stub "mblen"

stubs_end
