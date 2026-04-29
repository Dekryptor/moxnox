#include "dll_stubs.S"

stubs_start

do_stub_no_export "tcflush"
do_stub_no_export "nanosleep64"
do_stub_no_export "nanosleep"
do_stub_no_export "sleep"
do_stub_no_export "gettimeofday"
do_stub_no_export "realpath"

stubs_end
