#include "../../modules/stubs.S"

stubs_start

do_stub "GetLastError"
do_stub "TlsGetValue"
do_stub "VirtualProtect"
do_stub "VirtualQuery"
do_stub "FormatMessageA"
do_stub "LocalFree"
do_stub "SleepConditionVariableCS"
do_stub "GetSystemTimeAsFileTime"
do_stub "CloseHandle"
do_stub "GetThreadId"
do_stub "TlsAlloc"
do_stub "TlsFree"
do_stub "TlsSetValue"
do_stub "RaiseException"
do_stub "IsProcessorFeaturePresent"
do_stub "GetCurrentProcess"
do_stub "TerminateProcess"

stubs_end
