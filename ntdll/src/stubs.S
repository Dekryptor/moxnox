#include "dll_stubs.S"

stubs_start

do_stub "RtlTryEnterCriticalSection"
do_stub "RtlAddVectoredExceptionHandler"
do_stub "RtlRestoreContext"
do_stub "strncat"
do_stub "wcscpy"
do_stub "wcsncmp"

stubs_end
