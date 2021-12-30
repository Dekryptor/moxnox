#include "kernel.h"
#include "nt.h"

extern "C"
NTSTATUS user_NtDisplayString(const UNICODE_STRING* String) {
    // FIXME - get arguments from userspace
    // FIXME - call substantial function

    print("user_NtDisplayString\n");

    return nt_status::SUCCESS;
}
