#include "dll_stubs.S"

stubs_start

do_stub "VirtualProtect"
do_stub "VirtualQuery"
do_stub "FormatMessageA"
do_stub "LocalFree"
do_stub "SleepConditionVariableCS"
do_stub "GetSystemTimeAsFileTime"
do_stub "CloseHandle"
do_stub "GetThreadId"
do_stub "TlsFree"
do_stub "RaiseException"
do_stub "IsProcessorFeaturePresent"
do_stub "GetCurrentProcess"
do_stub "TerminateProcess"
do_stub "WideCharToMultiByte"
do_stub "MultiByteToWideChar"
do_stub "CreateProcessW"

stubs_end
