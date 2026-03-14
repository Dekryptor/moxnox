#include "dll_stubs.S"

stubs_start

do_stub "RtlTryEnterCriticalSection"
do_stub "RtlAddVectoredExceptionHandler"
do_stub "strnlen"
do_stub "RtlLookupFunctionEntry"
do_stub "RtlUnwindEx"
do_stub "RtlVirtualUnwind"
do_stub "iswctype"
do_stub "towlower"
do_stub "towupper"

stubs_end
