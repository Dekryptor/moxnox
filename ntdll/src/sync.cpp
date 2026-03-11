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
