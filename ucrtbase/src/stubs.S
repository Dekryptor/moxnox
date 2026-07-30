#include "dll_stubs.S"

stubs_start

do_stub "_beginthreadex"
do_stub "wcstod"
do_stub "wcsftime"
do_stub "wcsxfrm"
do_stub "asctime"
do_stub "fgetpos"
do_stub "fsetpos"
do_stub "wcscat"
do_stub "_crt_at_quick_exit"
do_stub "quick_exit"
do_stub "_timespec64_get"
do_stub "wcstof"
do_stub "wcstold"

stubs_end
