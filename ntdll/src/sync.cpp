#include <stddef.h>

import nt;
import ntdll;
import print;

extern "C"
void RtlInitializeConditionVariable(RTL_CONDITION_VARIABLE* var) {
    var->Ptr = nullptr;
}

extern "C"
void RtlWakeAllConditionVariable(RTL_CONDITION_VARIABLE* /*var*/) {
    print("FIXME - RtlWakeAllConditionVariable\n"); // FIXME
}

extern "C"
nt_status RtlSleepConditionVariableCS(RTL_CONDITION_VARIABLE* /*variable*/,
                                      RTL_CRITICAL_SECTION* /*crit*/,
                                      const LARGE_INTEGER* /*timeout*/) {
    print("FIXME - RtlSleepConditionVariableCS\n");

    return nt_status::NOT_IMPLEMENTED;
}

extern "C"
nt_status RtlWaitOnAddress(const void* /*addr*/, const void* /*cmp*/,
                           size_t /*size*/, const LARGE_INTEGER* /*timeout*/) {
    print("FIXME - RtlWaitOnAddress\n");

    return nt_status::NOT_IMPLEMENTED;
}
