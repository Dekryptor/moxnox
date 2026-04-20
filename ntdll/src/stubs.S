#include "dll_stubs.S"

stubs_start

do_stub "RtlTryEnterCriticalSection"
do_stub "RtlAddVectoredExceptionHandler"
do_stub "strnlen"
do_stub "iswctype"
do_stub "towlower"
do_stub "towupper"
do_stub "RtlTryAcquireSRWLockExclusive"
do_stub "RtlQueryPerformanceCounter"
do_stub "RtlQueryPerformanceFrequency"
do_stub "RtlRestoreContext"
do_stub "strncat"

stubs_end
