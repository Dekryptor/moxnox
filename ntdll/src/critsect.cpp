import nt;

extern "C"
void RtlInitializeCriticalSection(RTL_CRITICAL_SECTION* critical_section) {
    auto& cs = *critical_section;

    cs.DebugInfo = nullptr;
    cs.LockCount = -1;
    cs.RecursionCount = 0;
    cs.OwningThread = 0;
    cs.LockSemaphore = 0;
    cs.SpinCount = 4000; // FIXME - 0 if single-CPU machine
}

extern "C"
void RtlDeleteCriticalSection(RTL_CRITICAL_SECTION* /*critical_section*/) {
    // FIXME
}

extern "C"
void RtlEnterCriticalSection(RTL_CRITICAL_SECTION* /*critical_section*/) {
    // FIXME
}

extern "C"
void RtlLeaveCriticalSection(RTL_CRITICAL_SECTION* /*critical_section*/) {
    // FIXME
}
