#include "dll_stubs.S"

stubs_start

do_stub "RtlTryEnterCriticalSection"
do_stub "RtlAddVectoredExceptionHandler"
do_stub "strnlen"
do_stub "RtlVirtualUnwind"
do_stub "iswctype"
do_stub "towlower"
do_stub "towupper"

#ifndef __x86_64__
do_stub "RtlUnwindEx"
#endif

stubs_end
