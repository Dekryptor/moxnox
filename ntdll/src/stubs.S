#include "dll_stubs.S"

stubs_start

do_stub "RtlTryEnterCriticalSection"
do_stub "RtlAddVectoredExceptionHandler"
do_stub "RtlRestoreContext"
do_stub "RtlAcquireSRWLockShared"
do_stub "RtlReleaseSRWLockShared"
do_stub "RtlTryAcquireSRWLockShared"

stubs_end
