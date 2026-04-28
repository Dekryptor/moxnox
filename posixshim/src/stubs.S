#include "dll_stubs.S"

stubs_start

do_stub_no_export "cfgetospeed"
do_stub_no_export "tcflush"
do_stub_no_export "tcsetattr"
do_stub_no_export "nanosleep64"
do_stub_no_export "nanosleep"
do_stub_no_export "sleep"

stubs_end
