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
do_stub "_execv"
do_stub "_execle"
do_stub "_mktemp"
do_stub "mbstowcs"
do_stub "fputwc"
do_stub "wcschr"
do_stub "wcsrchr"
do_stub "wcstok"
do_stub "wcstombs"
do_stub "_tzset"
do_stub "lldiv"
do_stub "__timezone"

stubs_end
