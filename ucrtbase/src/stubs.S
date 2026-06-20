#include "dll_stubs.S"

stubs_start

do_stub "___lc_codepage_func"
do_stub "_beginthreadex"
do_stub "wcstol"
do_stub "wcstoul"
do_stub "wcstoll"
do_stub "wcstoull"
do_stub "wcstod"
do_stub "_wcsnicmp"
do_stub "_execvp"
do_stub "_execl"
do_stub "_execlp"
do_stub "_execve"
do_stub "_mktemp"
do_stub "mbstowcs"
do_stub "fputwc"

stubs_end
