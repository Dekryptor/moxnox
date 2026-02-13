#include "../../modules/stubs.S"

stubs_start

do_stub "RtlTryEnterCriticalSection"
do_stub "RtlSetLastWin32Error"
do_stub "RtlInitializeConditionVariable"
do_stub "RtlWakeAllConditionVariable"
do_stub "RtlWakeConditionVariable"
do_stub "RtlAddVectoredExceptionHandler"
do_stub "strnlen"

stubs_end
