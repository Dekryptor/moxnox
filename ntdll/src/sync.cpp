import nt;
import ntdll;
import print;

extern "C"
void RtlInitializeConditionVariable(RTL_CONDITION_VARIABLE* /*var*/) {
    print("FIXME - RtlInitializeConditionVariable\n"); // FIXME
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
