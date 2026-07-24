#include "dll_stubs.S"

stubs_start

do_stub "___lc_codepage_func"
do_stub "_beginthreadex"
do_stub "wcstod"
do_stub "fputwc"
do_stub "getwc"
do_stub "putwc"
do_stub "strxfrm"
do_stub "ungetwc"
do_stub "wcscoll"
do_stub "wcsftime"
do_stub "wcsxfrm"
do_stub "asctime"
do_stub "fgetpos"
do_stub "fsetpos"
do_stub "mbsrtowcs"
do_stub "wcscat"
do_stub "atoll"
do_stub "_crt_at_quick_exit"
do_stub "quick_exit"
do_stub "_timespec64_get"

stubs_end
